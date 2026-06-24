// GridBot — multi-peer ESP-NOW tournament lobby (the bar/classroom "Cup across N devices" mode).
// SEPARATE from net::Radio (the 1:1 trade/battle link) on purpose, so it can never break trade
// or a normal radio battle -- only one of the two is active at a time (each registers its own
// esp_now recv callback when it starts). Reuses net::BotCard for each player's fighter.
//
// Protocol (every packet <= 250 bytes, the ESP-NOW limit; cards are chunked):
//   HOST opens a lobby and periodically broadcasts HELLO{lobbyId}. PEERs that hear it send their
//   BotCard (JOIN, chunked). The host collects a de-duped roster (by uuid) and re-broadcasts it
//   (ROSTER) so every device shows the same player list. On "Start", the host broadcasts SEED
//   {seed, sumo, playerCount}; because the Arena is fully deterministic, every device then builds
//   the SAME bracket from the shared seed + roster and replays each match locally -- no frame
//   streaming. Devices report match outcomes (RESULT) and the host broadcasts round ADVANCE.
//   (RESULT/ADVANCE + the local replay are driven by the lobby screen; this layer owns lobby +
//   roster + seed handshake.)
//
// HARDWARE-PENDING: needs 2+ physical boards to verify end-to-end. Compiles clean; the roster
// handshake is implemented, the match-replay loop is the screen's job (marked where it hooks in).
#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <vector>
#include "net/Radio.h"   // BotCard

namespace net {

constexpr int TOURNEY_MAX = 16;   // up to 16 devices in a room (matches gb::BRACKET_MAX)

enum class TRole  : uint8_t { NONE, HOST, PEER };
enum class TState : uint8_t { IDLE, LOBBY, SEEDED, FAILED };

class TourneyNet {
 public:
  bool begin();                          // WiFi STA + esp_now init + our recv callback
  void host(const BotCard& mine);        // open a lobby (become HOST); I'm player 0
  void join(const BotCard& mine);        // look for a lobby and join it (become PEER)
  void start(uint32_t seed, bool sumo);  // HOST only: lock roster + broadcast the bracket seed
  void leave();                          // tear down (re-registering Radio is the caller's job)
  void loop(uint32_t now);               // pump HELLO/ROSTER broadcasts + chunked sends

  TRole  role()  const { return _role; }
  TState state() const { return _state; }
  bool   isHost() const { return _role == TRole::HOST; }
  int    playerCount() const { return (int)_roster.size(); }
  const std::vector<BotCard>& roster() const { return _roster; }
  bool     seeded() const { return _state == TState::SEEDED; }
  uint32_t seed() const { return _seed; }
  bool     sumo() const { return _sumo; }

  void onRecv(const uint8_t* mac, const uint8_t* data, int len);  // from the static trampoline

 private:
  void addPeerMac(const uint8_t* mac);
  void broadcastHello(uint32_t now);
  void sendMyCard();                       // broadcast _mine, chunked (BEGIN + CHUNKs + END)
  int  rosterIndexByUuid(const String& uuid) const;
  void ingestCard(const BotCard& c);       // add/replace a roster entry (de-dupe by uuid)
  // per-sender reassembly: cards from different boards interleave on the broadcast, so each
  // incoming transfer is keyed by the sender's MAC into its own slot until END.
  struct RxSlot { uint8_t mac[6]; bool active = false; uint16_t total = 0; String buf; BotCard meta; };
  int rxSlotFor(const uint8_t* mac);       // find/allocate the reassembly slot for this sender

  TRole  _role = TRole::NONE;
  TState _state = TState::IDLE;
  BotCard _mine;
  std::vector<BotCard> _roster;            // player 0 = host, then join order
  uint8_t _host[6] = {0};                  // host MAC (peers learn it from HELLO)
  bool _haveHost = false;
  uint32_t _lobbyId = 0;                   // distinguishes overlapping lobbies in one room
  uint32_t _seed = 0;
  bool _sumo = true;
  uint32_t _lastBeacon = 0;                // last HELLO/card broadcast
  static constexpr int RX_SLOTS = 4;       // simultaneous incoming card transfers (small room)
  RxSlot _rx[RX_SLOTS];
};

// Lazily heap-allocated singleton: costs no static DRAM until a lobby screen first asks for it
// (the rest of the firmware is at the static-DRAM limit, and this is only used in the room mode).
TourneyNet& tourney();

}  // namespace net
