// GridBot — ad-hoc ESP-NOW link for two CYDs (SPEC §18.4, the parked radio stretch).
// Pairs over broadcast, then exchanges a "BotCard" (friend name + avatar + program
// AST as JSON). Because the Arena is fully deterministic (SPEC §18.1), both devices
// then run the SAME match from a shared seed (derived from both MACs) — no live sync.
// The same card exchange powers Pokemon-style TRADE (drop a friend's bot in your
// library). Board-only (esp_now); excluded from the native build.
//
// HARDWARE-PENDING: needs two physical boards to verify end-to-end. Compiles clean.
#pragma once
#include <Arduino.h>
#include <stdint.h>

namespace net {

struct BotCard {
  String name;       // friend's profile name (<= 8 chars)
  uint8_t avatar = 0;
  String progJson;   // program AST serialized via gb::programToJson
};

enum class RadioStatus : uint8_t { IDLE, SEARCHING, PAIRED, EXCHANGING, READY, FAILED };

class Radio {
 public:
  bool begin();                       // WiFi STA + esp_now init
  void startSession(const BotCard& mine);  // begin searching + offer my card
  void stop();
  void loop(uint32_t now);            // pump pairing + chunked send

  RadioStatus status() const { return _status; }
  bool ready() const { return _status == RadioStatus::READY; }
  const BotCard& theirCard() const { return _their; }
  uint32_t sharedSeed() const { return _seed; }
  bool iAmBot0() const { return _iAmBot0; }  // deterministic role from MAC order

  // internal (called from static esp_now trampolines)
  void onRecv(const uint8_t* mac, const uint8_t* data, int len);

 private:
  void broadcastHello();
  void lockPeer(const uint8_t* mac);
  void sendCardChunks();
  void computeSeedAndRole();

  RadioStatus _status = RadioStatus::IDLE;
  BotCard _mine, _their;
  uint8_t _peer[6] = {0};
  bool _hasPeer = false;
  bool _myCardSent = false;
  bool _theirCardDone = false;
  uint32_t _seed = 0;
  bool _iAmBot0 = false;
  uint32_t _lastHello = 0;
  int _sendOffset = 0;
  bool _beginSent = false;
};

extern Radio radio;

}  // namespace net
