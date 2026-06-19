#include "game/Maze.h"

namespace gb {

void Maze::reset(int rows, int cols) {
  if (rows > MAZE_MAX_ROWS) rows = MAZE_MAX_ROWS;
  if (cols > MAZE_MAX_COLS) cols = MAZE_MAX_COLS;
  _rows = rows;
  _cols = cols;
  for (int i = 0; i < rows * cols; i++) _tiles[i] = (uint8_t)FLOOR;
  _start = Pose{};
  _goalR = _goalC = 0;
}

void Maze::fill(Tile t) {
  for (int i = 0; i < _rows * _cols; i++) _tiles[i] = (uint8_t)t;
}

}  // namespace gb
