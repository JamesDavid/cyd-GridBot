#include "net/TourneyNet.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

namespace net {

static TourneyNet* s_tn = nullptr;          // the heap singleton (see tourney())
TourneyNet& tourney() { if (!s_tn) s_tn = new TourneyNet(); return *s_tn; }

// Wire protocol. Each packet <= 250 bytes (ESP-NOW limit). A card that doesn't fit one packet is
// a TODO (chunk it like net::Radio does) -- marked below; small coded bots fit fine.
enum : uint8_t { TM_HELLO = 1, TM_CARD = 2, TM_SEED = 3 };
static const uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const char SEP = '\x1F';   // field separator inside a packed card

static void recvTrampoline(const uint8_t* mac, const uint8_t* data, int len) {
  if (s_tn) s_tn->onRecv(mac, data, len);
}

void TourneyNet::addPeerMac(const uint8_t* mac) {
  if (esp_now_is_peer_exist(mac)) return;
  esp_now_peer_info_t p = {};
  memcpy(p.peer_addr, mac, 6);
  p.channel = 0; p.encrypt = false;
  esp_now_add_peer(&p);
}

bool TourneyNet::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() != ESP_OK) { _state = TState::FAILED; return false; }
  esp_now_register_recv_cb(recvTrampoline);   // NOTE: global -- supersedes net::Radio's while active
  addPeerMac(BCAST);
  _state = TState::IDLE;
  return true;
}

// --- (de)serialize a BotCard to a single field-separated blob (fits one packet when small) ---
static String packCard(const BotCard& c) {
  String s = c.name; s += SEP; s += String((int)c.avatar); s += SEP;
  s += c.uuid; s += SEP; s += c.botName; s += SEP; s += c.progJson;
  return s;
}
static bool unpackCard(const uint8_t* data, int len, BotCard& c) {
  String s; s.reserve(len); for (int i = 0; i < len; i++) s += (char)data[i];
  int f = 0, start = 0; String fields[5];
  for (int i = 0; i <= s.length() && f < 5; i++) {
    if (i == s.length() || s[i] == SEP) { fields[f++] = s.substring(start, i); start = i + 1; }
  }
  if (f < 5) return false;
  c.name = fields[0]; c.avatar = (uint8_t)fields[1].toInt();
  c.uuid = fields[2]; c.botName = fields[3]; c.progJson = fields[4];
  return true;
}

int TourneyNet::rosterIndexByUuid(const String& uuid) const {
  for (int i = 0; i < (int)_roster.size(); i++) if (_roster[i].uuid == uuid) return i;
  return -1;
}

// Add or replace a player, keeping the roster sorted by uuid so EVERY device ends up with the
// identical order -> the same bracket from the same shared seed (no central ordering authority).
void TourneyNet::ingestCard(const BotCard& c) {
  int at = rosterIndexByUuid(c.uuid);
  if (at >= 0) { _roster[at] = c; return; }
  if ((int)_roster.size() >= TOURNEY_MAX) return;
  size_t pos = 0; while (pos < _roster.size() && _roster[pos].uuid < c.uuid) pos++;
  _roster.insert(_roster.begin() + pos, c);
}

void TourneyNet::host(const BotCard& mine) {
  _role = TRole::HOST; _state = TState::LOBBY;
  _mine = mine; _roster.clear(); ingestCard(mine);
  esp_wifi_get_mac(WIFI_IF_STA, _host); _haveHost = true;
  uint8_t m[6]; esp_wifi_get_mac(WIFI_IF_STA, m);
  _lobbyId = ((uint32_t)m[5] << 8) | m[4] | (millis() & 0xFFFF0000u);  // room-unique-ish id
  _seed = 0; _lastBeacon = 0;
}

void TourneyNet::join(const BotCard& mine) {
  _role = TRole::PEER; _state = TState::LOBBY;
  _mine = mine; _roster.clear(); ingestCard(mine);
  _haveHost = false; _seed = 0; _lastBeacon = 0;
}

void TourneyNet::start(uint32_t seed, bool sumo) {
  if (_role != TRole::HOST) return;
  _seed = seed; _sumo = sumo; _state = TState::SEEDED;
  uint8_t pkt[7] = {TM_SEED, (uint8_t)(seed), (uint8_t)(seed >> 8), (uint8_t)(seed >> 16),
                    (uint8_t)(seed >> 24), (uint8_t)(sumo ? 1 : 0), (uint8_t)_roster.size()};
  esp_now_send(BCAST, pkt, sizeof(pkt));
}

void TourneyNet::leave() { _role = TRole::NONE; _state = TState::IDLE; _roster.clear(); _haveHost = false; }

void TourneyNet::broadcastHello(uint32_t now) {
  uint8_t pkt[5] = {TM_HELLO, (uint8_t)(_lobbyId), (uint8_t)(_lobbyId >> 8),
                    (uint8_t)(_lobbyId >> 16), (uint8_t)(_lobbyId >> 24)};
  esp_now_send(BCAST, pkt, sizeof(pkt));
}

void TourneyNet::sendMyCard(const uint8_t* /*toMac*/) {
  String s = packCard(_mine);
  if (s.length() > 240) return;   // TODO: chunk large (neural) cards like net::Radio's M_CHUNK path
  uint8_t pkt[241]; pkt[0] = TM_CARD;
  memcpy(pkt + 1, s.c_str(), s.length());
  esp_now_send(BCAST, pkt, 1 + s.length());
}

void TourneyNet::loop(uint32_t now) {
  if (_state != TState::LOBBY) return;
  if (now - _lastBeacon < 1200) return;
  _lastBeacon = now;
  if (_role == TRole::HOST) broadcastHello(now);  // announce the lobby
  sendMyCard(nullptr);                            // everyone rebroadcasts their card -> roster converges
}

void TourneyNet::onRecv(const uint8_t* mac, const uint8_t* data, int len) {
  if (len < 1) return;
  switch (data[0]) {
    case TM_HELLO:
      if (_role == TRole::PEER && !_haveHost) { memcpy(_host, mac, 6); _haveHost = true; addPeerMac(mac); sendMyCard(mac); }
      break;
    case TM_CARD: {
      BotCard c;
      if (unpackCard(data + 1, len - 1, c) && _state == TState::LOBBY) ingestCard(c);
      break;
    }
    case TM_SEED:
      if (len >= 7 && _role == TRole::PEER) {
        _seed = (uint32_t)data[1] | ((uint32_t)data[2] << 8) | ((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 24);
        _sumo = data[5] != 0;
        _state = TState::SEEDED;   // the lobby screen now builds the bracket from _seed + sorted roster
      }
      break;
    default: break;
  }
}

}  // namespace net
