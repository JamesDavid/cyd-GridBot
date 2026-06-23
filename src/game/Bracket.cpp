#include "game/Bracket.h"

namespace gb {

void Bracket::init(int numPlayers) {
  if (numPlayers < 1) numPlayers = 1;
  if (numPlayers > BRACKET_MAX) numPlayers = BRACKET_MAX;
  n = numPlayers;
  int s = 1;
  while (s < n) s <<= 1;                 // next power of two >= n
  nSlots = s;
  round = 0;
  for (int i = 0; i < BRACKET_MAX; i++) slot[i] = -1;
  for (int i = 0; i < BRACKET_MAX / 2; i++) win[i] = -1;
  if (nSlots == 1) { slot[0] = 0; return; }   // lone player is the champion
  // Distribute byes so EACH match is either player-vs-player or player-vs-bye (never bye-vs-bye):
  // the first `realMatches` pairs get two players, the remaining pairs get one player + a bye.
  int pairs = nSlots / 2;
  int byes = nSlots - n;
  int realMatches = pairs - byes; if (realMatches < 0) realMatches = 0;
  int pid = 0;
  for (int m = 0; m < pairs; m++) {
    slot[2 * m] = (int16_t)pid++;
    if (m < realMatches) slot[2 * m + 1] = (int16_t)pid++;   // else slot[2m+1] stays -1 (bye)
  }
}

void Bracket::pair(int m, int& a, int& b) const {
  a = b = -1;
  if (m < 0 || m >= matchCount()) return;
  a = slot[2 * m];
  b = slot[2 * m + 1];
  if (a < 0 && b >= 0) { a = b; b = -1; }   // normalise: the present player is always 'a'
}

void Bracket::reportWinner(int m, int playerId) {
  if (m < 0 || m >= matchCount()) return;
  win[m] = (int16_t)playerId;
}

bool Bracket::roundComplete() const {
  for (int m = 0; m < matchCount(); m++) {
    int a, b; pair(m, a, b);
    if (b < 0) continue;          // a bye is auto-decided
    if (win[m] < 0) return false; // a real match still pending
  }
  return true;
}

void Bracket::advance() {
  int half = nSlots / 2;
  int16_t next[BRACKET_MAX];
  for (int m = 0; m < half; m++) {
    int a, b; pair(m, a, b);
    next[m] = (b < 0) ? (int16_t)a : win[m];   // bye -> a; else the reported winner
  }
  nSlots = half;
  for (int i = 0; i < nSlots; i++) slot[i] = next[i];
  for (int i = 0; i < BRACKET_MAX / 2; i++) win[i] = -1;
  round++;
}

}  // namespace gb
