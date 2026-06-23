// GridBot — single-elimination tournament bracket (the bar/classroom mode's scheduler).
// Pure logic, no networking: seed N players, pair them each round, record winners, advance
// until one champion remains. Byes are handled for non-power-of-two fields. Host-tested;
// the networked TourneyScreen drives it and broadcasts each round's pairings + results.
#pragma once
#include <stdint.h>

namespace gb {

constexpr int BRACKET_MAX = 16;   // up to 16 devices in a room

struct Bracket {
  int n = 0;                      // players entered (0..BRACKET_MAX)
  int nSlots = 0;                 // slots this round = next power of two >= survivors
  int round = 0;                  // 0-based round index
  int16_t slot[BRACKET_MAX];      // player id in each slot this round (-1 = empty/bye)
  int16_t win[BRACKET_MAX / 2];   // reported winner (player id) per match this round (-1 = pending)

  // Seed players 0..numPlayers-1 into round 0. Byes pad the field up to a power of two and
  // are placed so the late seeds (highest ids) draw them, like a normal seeded bracket.
  void init(int numPlayers);

  int  matchCount() const { return nSlots / 2; }      // matches in the current round
  // The two player ids in match m: a always present; b == -1 means a gets a BYE (auto-advance).
  void pair(int m, int& a, int& b) const;
  void reportWinner(int m, int playerId);             // record the winner of match m
  bool roundComplete() const;                          // every match (incl. byes) decided
  void advance();                                      // winners -> next round's slots
  bool done() const { return nSlots <= 1; }            // a single survivor remains
  int  champion() const { return nSlots >= 1 ? slot[0] : -1; }  // valid once done()
};

}  // namespace gb
