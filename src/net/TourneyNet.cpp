#include "net/TourneyNet.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

namespace net {

static TourneyNet* s_tn = nullptr;          // the heap singleton (see tourney())
TourneyNet& tourney() { if (!s_tn) s_tn = new TourneyNet(); return *s_tn; }

// Wire protocol. Each packet <= 250 bytes (ESP-NOW limit). Cards are CHUNKED so a full / neural
// fighter (bigger than one packet) rides the lobby; transfers are reassembled per-sender MAC.
//   CBEGIN: [2][total(2)][avatar(1)][nameLen(1)][name][uuidLen(1)][uuid][botNameLen(1)][botName]
//   CCHUNK: [3][off(2)][progJson bytes...]      CEND: [4]
enum : uint8_t { TM_HELLO = 1, TM_CBEGIN = 2, TM_CCHUNK = 3, TM_CEND = 4, TM_SEED = 5 };
static const uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static constexpr int TM_CHUNK = 220;   // progJson bytes per CCHUNK packet

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

static constexpr uint8_t TN_CHANNEL = 6;   // every board pins the same channel so broadcasts are heard

bool TourneyNet::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_now_init();   // ESP_OK, or already-initialized if net::Radio set it up first -- both fine
  esp_now_register_recv_cb(recvTrampoline);   // global -- supersedes net::Radio's recv cb while active
  addPeerMac(BCAST);
  // Pin a fixed WiFi channel -- ESP-NOW broadcast only reaches peers on the SAME channel, and
  // without an AP each board would otherwise sit on its own default channel and hear nothing.
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(TN_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  _state = TState::IDLE;
  return true;
}

// Find the reassembly slot for this sender (or claim a free/oldest one).
int TourneyNet::rxSlotFor(const uint8_t* mac) {
  for (int i = 0; i < RX_SLOTS; i++) if (_rx[i].active && memcmp(_rx[i].mac, mac, 6) == 0) return i;
  for (int i = 0; i < RX_SLOTS; i++) if (!_rx[i].active) return i;
  return 0;  // all busy -> reuse slot 0 (small room; transfers are short)
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
  for (int i = 0; i < 8; i++) { esp_now_send(BCAST, pkt, sizeof(pkt)); delay(15); }  // repeat: one SEED packet can drop
}

void TourneyNet::leave() { _role = TRole::NONE; _state = TState::IDLE; _roster.clear(); _haveHost = false; }

void TourneyNet::broadcastHello(uint32_t now) {
  uint8_t pkt[5] = {TM_HELLO, (uint8_t)(_lobbyId), (uint8_t)(_lobbyId >> 8),
                    (uint8_t)(_lobbyId >> 16), (uint8_t)(_lobbyId >> 24)};
  esp_now_send(BCAST, pkt, sizeof(pkt));
}

// Broadcast my card in a quick burst: CBEGIN (meta) + CCHUNK(progJson) + CEND.
void TourneyNet::sendMyCard() {
  uint8_t pkt[TM_CHUNK + 8]; int n = 0;
  uint16_t total = (uint16_t)_mine.progJson.length();
  pkt[n++] = TM_CBEGIN; pkt[n++] = total & 0xFF; pkt[n++] = total >> 8; pkt[n++] = _mine.avatar;
  uint8_t nl = (uint8_t)min((int)_mine.name.length(), 8);    pkt[n++] = nl; for (int i = 0; i < nl; i++) pkt[n++] = _mine.name[i];
  uint8_t ul = (uint8_t)min((int)_mine.uuid.length(), 16);   pkt[n++] = ul; for (int i = 0; i < ul; i++) pkt[n++] = _mine.uuid[i];
  uint8_t bl = (uint8_t)min((int)_mine.botName.length(), 16); pkt[n++] = bl; for (int i = 0; i < bl; i++) pkt[n++] = _mine.botName[i];
  esp_now_send(BCAST, pkt, n); delay(8);
  for (int off = 0; off < total; off += TM_CHUNK) {
    int take = min(TM_CHUNK, total - off);
    int m = 0; uint8_t cp[TM_CHUNK + 4];
    cp[m++] = TM_CCHUNK; cp[m++] = off & 0xFF; cp[m++] = (off >> 8) & 0xFF;
    for (int i = 0; i < take; i++) cp[m++] = _mine.progJson[off + i];
    esp_now_send(BCAST, cp, m); delay(8);
  }
  uint8_t e[1] = {TM_CEND}; esp_now_send(BCAST, e, 1);
}

void TourneyNet::loop(uint32_t now) {
  if (_state != TState::LOBBY) return;
  if (now - _lastBeacon < 1400) return;
  _lastBeacon = now;
  if (_role == TRole::HOST) broadcastHello(now);  // announce the lobby
  sendMyCard();                                   // everyone rebroadcasts their card -> roster converges
}

void TourneyNet::onRecv(const uint8_t* mac, const uint8_t* data, int len) {
  if (len < 1) return;
  switch (data[0]) {
    case TM_HELLO:
      if (_role == TRole::PEER && !_haveHost) { memcpy(_host, mac, 6); _haveHost = true; addPeerMac(mac); sendMyCard(); }
      break;
    case TM_CBEGIN: {                              // start a card transfer from this sender
      if (len < 5) return;
      int s = rxSlotFor(mac);
      RxSlot& r = _rx[s];
      memcpy(r.mac, mac, 6); r.active = true; r.buf = "";
      r.total = data[1] | (data[2] << 8);
      r.meta = BotCard{}; r.meta.avatar = data[3];
      int p = 4;
      uint8_t nl = (p < len) ? data[p++] : 0; for (int i = 0; i < nl && p < len; i++) r.meta.name += (char)data[p++];
      uint8_t ul = (p < len) ? data[p++] : 0; for (int i = 0; i < ul && p < len; i++) r.meta.uuid += (char)data[p++];
      uint8_t bl = (p < len) ? data[p++] : 0; for (int i = 0; i < bl && p < len; i++) r.meta.botName += (char)data[p++];
      break;
    }
    case TM_CCHUNK: {                              // append bytes to this sender's buffer
      if (len < 3) return;
      int s = rxSlotFor(mac); RxSlot& r = _rx[s];
      if (!r.active || memcmp(r.mac, mac, 6) != 0) break;
      for (int i = 3; i < len; i++) r.buf += (char)data[i];
      break;
    }
    case TM_CEND: {                               // card complete -> ingest it
      int s = rxSlotFor(mac); RxSlot& r = _rx[s];
      if (r.active && memcmp(r.mac, mac, 6) == 0 && (int)r.buf.length() >= r.total && _state == TState::LOBBY) {
        r.meta.progJson = r.buf; ingestCard(r.meta);
      }
      r.active = false; r.buf = "";
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
