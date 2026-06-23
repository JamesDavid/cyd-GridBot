#include "net/Radio.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "core/Util.h"

namespace net {

Radio radio;

// Wire protocol (each packet <= 250 bytes, the ESP-NOW limit).
enum : uint8_t { M_HELLO = 1, M_BEGIN = 2, M_CHUNK = 3, M_END = 4 };
static const uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static constexpr int CHUNK = 200;

static void recvTrampoline(const uint8_t* mac, const uint8_t* data, int len) {
  radio.onRecv(mac, data, len);
}

static void addPeer(const uint8_t* mac) {
  if (esp_now_is_peer_exist(mac)) return;
  esp_now_peer_info_t p = {};
  memcpy(p.peer_addr, mac, 6);
  p.channel = 0;
  p.encrypt = false;
  esp_now_add_peer(&p);
}

bool Radio::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() != ESP_OK) { _status = RadioStatus::FAILED; return false; }
  esp_now_register_recv_cb(recvTrampoline);
  addPeer(BCAST);
  _status = RadioStatus::IDLE;
  return true;
}

void Radio::startSession(const BotCard& mine) {
  _mine = mine;
  _their = BotCard{};
  _hasPeer = false;
  _myCardSent = false;
  _theirCardDone = false;
  _beginSent = false;
  _sendOffset = 0;
  _seed = 0;
  _status = RadioStatus::SEARCHING;
  _lastHello = 0;
}

void Radio::stop() {
  _status = RadioStatus::IDLE;
  _hasPeer = false;
}

void Radio::broadcastHello() {
  uint8_t pkt[1] = {M_HELLO};
  esp_now_send(BCAST, pkt, sizeof(pkt));
}

void Radio::lockPeer(const uint8_t* mac) {
  memcpy(_peer, mac, 6);
  addPeer(mac);
  _hasPeer = true;
  _status = RadioStatus::EXCHANGING;
  computeSeedAndRole();
}

void Radio::computeSeedAndRole() {
  uint8_t self[6] = {0};
  esp_wifi_get_mac(WIFI_IF_STA, self);
  // Deterministic role: the lexicographically smaller MAC is bot0 (both agree).
  int cmp = memcmp(self, _peer, 6);
  _iAmBot0 = (cmp < 0);
  uint32_t h = 2166136261u;
  const uint8_t* lo = (cmp < 0) ? self : _peer;
  const uint8_t* hi = (cmp < 0) ? _peer : self;
  for (int i = 0; i < 6; i++) h = (h ^ lo[i]) * 16777619u;
  for (int i = 0; i < 6; i++) h = (h ^ hi[i]) * 16777619u;
  _seed = gb::mix32(h);
}

void Radio::sendCardChunks() {
  if (!_hasPeer) return;
  if (!_beginSent) {
    // [M_BEGIN][u16 totalLen][u8 avatar][u8 nameLen][name...][u8 uuidLen][uuid...][u8 botNameLen][botName...]
    uint8_t pkt[96];
    int n = 0;
    pkt[n++] = M_BEGIN;
    uint16_t total = (uint16_t)_mine.progJson.length();
    pkt[n++] = total & 0xFF; pkt[n++] = total >> 8;
    pkt[n++] = _mine.avatar;
    uint8_t nl = (uint8_t)min((int)_mine.name.length(), 8);
    pkt[n++] = nl;
    for (int i = 0; i < nl; i++) pkt[n++] = _mine.name[i];
    uint8_t ul = (uint8_t)min((int)_mine.uuid.length(), 32);
    pkt[n++] = ul;
    for (int i = 0; i < ul; i++) pkt[n++] = _mine.uuid[i];
    uint8_t bnl = (uint8_t)min((int)_mine.botName.length(), 16);  // the bot's own name
    pkt[n++] = bnl;
    for (int i = 0; i < bnl; i++) pkt[n++] = _mine.botName[i];
    esp_now_send(_peer, pkt, n);
    _beginSent = true;
    _sendOffset = 0;
    return;
  }
  int len = _mine.progJson.length();
  if (_sendOffset < len) {
    uint8_t pkt[CHUNK + 8];
    int n = 0;
    pkt[n++] = M_CHUNK;
    uint16_t off = (uint16_t)_sendOffset;
    pkt[n++] = off & 0xFF; pkt[n++] = off >> 8;
    int take = min(CHUNK, len - _sendOffset);
    for (int i = 0; i < take; i++) pkt[n++] = _mine.progJson[_sendOffset + i];
    esp_now_send(_peer, pkt, n);
    _sendOffset += take;
  } else {
    uint8_t pkt[1] = {M_END};
    esp_now_send(_peer, pkt, sizeof(pkt));
    _myCardSent = true;
  }
}

void Radio::onRecv(const uint8_t* mac, const uint8_t* data, int len) {
  if (len < 1) return;
  switch (data[0]) {
    case M_HELLO:
      if (!_hasPeer && _status == RadioStatus::SEARCHING) {
        lockPeer(mac);
        broadcastHello();  // help the peer lock onto us too
      }
      break;
    case M_BEGIN: {
      if (len < 5) return;
      if (!_hasPeer) lockPeer(mac);
      uint16_t total = data[1] | (data[2] << 8);
      _their.avatar = data[3];
      uint8_t nl = data[4];
      _their.name = "";
      int p = 5;
      for (int i = 0; i < nl && p < len; i++) _their.name += (char)data[p++];
      _their.uuid = "";
      if (p < len) {
        uint8_t ul = data[p++];
        for (int i = 0; i < ul && p < len; i++) _their.uuid += (char)data[p++];
      }
      _their.botName = "";
      if (p < len) {
        uint8_t bnl = data[p++];
        for (int i = 0; i < bnl && p < len; i++) _their.botName += (char)data[p++];
      }
      _their.progJson = "";
      _their.progJson.reserve(total);
      _theirCardDone = false;
      break;
    }
    case M_CHUNK: {
      if (len < 3) return;
      // offset is informational; we append in arrival order (ESP-NOW is in-order on a link)
      for (int i = 3; i < len; i++) _their.progJson += (char)data[i];
      break;
    }
    case M_END:
      _theirCardDone = true;
      break;
  }
  if (_myCardSent && _theirCardDone) _status = RadioStatus::READY;
}

void Radio::loop(uint32_t now) {
  if (_status == RadioStatus::SEARCHING) {
    if (now - _lastHello >= 400) { broadcastHello(); _lastHello = now; }
  } else if (_status == RadioStatus::EXCHANGING) {
    sendCardChunks();
    if (_myCardSent && _theirCardDone) _status = RadioStatus::READY;
  }
}

}  // namespace net
