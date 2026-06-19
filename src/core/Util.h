// GridBot — platform-agnostic utilities (no Arduino, no LovyanGFX).
// Used by the engine (Maze, MazeGen, Interpreter, Arena) so it compiles under
// both env:cyd_gridbot and env:native. Determinism lives here: same inputs ->
// same outputs, which is what makes the headless tests possible (SPEC §5.2, §6).
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace gb {

// 32-bit integer hash (splitmix-style finalizer). Deterministic, fast.
inline uint32_t mix32(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352dU;
  x ^= x >> 15; x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}

// Per-level seed: seed = hash(profile.seedBase, levelNumber)  (SPEC §5.2).
inline uint32_t seedFor(uint32_t seedBase, uint32_t level) {
  return mix32(seedBase * 2654435761U + level * 40503U + 0x9E3779B9U);
}

// Tiny deterministic PRNG (xorshift32). NEVER used in the arena tick loop
// (SPEC §18.1) — only for maze generation, which is keyed by a level seed.
struct Rng {
  uint32_t s;
  explicit Rng(uint32_t seed) : s(seed ? seed : 0xDEADBEEFU) {}
  uint32_t next() {
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return s;
  }
  // Uniform-ish in [0, n).
  uint32_t below(uint32_t n) { return n ? (next() % n) : 0; }
  // True with probability pct/100.
  bool chance(uint32_t pct) { return below(100) < pct; }
};

template <typename T> inline T clampv(T v, T lo, T hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}
template <typename T> inline T minv(T a, T b) { return a < b ? a : b; }
template <typename T> inline T maxv(T a, T b) { return a > b ? a : b; }
template <typename T> inline T absv(T a) { return a < 0 ? -a : a; }

}  // namespace gb
