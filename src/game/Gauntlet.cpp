#include "game/Gauntlet.h"
#include "game/Maze.h"
#include "game/MazeGen.h"
#include "game/Program.h"
#include "game/Interpreter.h"

namespace gb {

int gauntletRun(const Net& brain, uint32_t seedBase, int upToLevel) {
  for (int lvl = 1; lvl <= upToLevel; lvl++) {
    Maze boards[MAX_BOARDS];
    int nb = MazeGen::generateBoards(boards, MAX_BOARDS, seedBase, lvl);
    for (int b = 0; b < nb; b++) {
      // The standard reactive policy: sense -> brain -> act, until the goal. No training.
      Program prog; prog.brains.push_back(brain);
      Node loop = Node::repeatUntil(AT_GOAL);
      loop.body.push_back(Node::neuro(0));
      prog.main.push_back(loop);
      Interpreter it; it.load(&prog, &boards[b], boards[b].startPose(), 500);
      if (it.runToEnd() != OUT_WIN) return lvl - 1;  // missed level `lvl`
    }
  }
  return upToLevel;
}

}  // namespace gb
