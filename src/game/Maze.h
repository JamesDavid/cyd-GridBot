// GridBot — runtime grid model (SPEC §5, §10). Fixed-capacity (no heap) so it's
// cheap on the no-PSRAM ESP32. Max grid is the SPEC §7 cap (~10x8 landscape).
#pragma once
#include "game/Types.h"

namespace gb {

constexpr int MAZE_MAX_COLS = 10;
constexpr int MAZE_MAX_ROWS = 8;
constexpr int MAZE_MAX_CELLS = MAZE_MAX_COLS * MAZE_MAX_ROWS;

class Maze {
 public:
  void reset(int rows, int cols);

  int rows() const { return _rows; }
  int cols() const { return _cols; }
  bool inBounds(int r, int c) const {
    return r >= 0 && r < _rows && c >= 0 && c < _cols;
  }

  Tile at(int r, int c) const {
    if (!inBounds(r, c)) return WALL;  // off-board reads as WALL (an edge)
    return (Tile)_tiles[r * _cols + c];
  }
  void set(int r, int c, Tile t) {
    if (inBounds(r, c)) _tiles[r * _cols + c] = (uint8_t)t;
  }
  void fill(Tile t);

  // Walkable for a normal Forward/Backward step (FLOOR/START/GOAL and decorations
  // that aren't blocking). WALL and PIT are not walkable.
  bool isWalkable(int r, int c) const {
    Tile t = at(r, c);
    return t != WALL && t != PIT;
  }

  const Pose& startPose() const { return _start; }
  void setStart(const Pose& p) { _start = p; }
  int goalRow() const { return _goalR; }
  int goalCol() const { return _goalC; }
  void setGoal(int r, int c) { _goalR = r; _goalC = c; set(r, c, GOAL); }
  void clearGoal() { if (inBounds(_goalR, _goalC)) set(_goalR, _goalC, FLOOR); _goalR = -1; _goalC = -1; }  // Sumo: no goal
  bool isGoal(int r, int c) const { return r == _goalR && c == _goalC; }

 private:
  uint8_t _tiles[MAZE_MAX_CELLS] = {0};
  int _rows = 0, _cols = 0;
  Pose _start;
  int _goalR = 0, _goalC = 0;
};

}  // namespace gb
