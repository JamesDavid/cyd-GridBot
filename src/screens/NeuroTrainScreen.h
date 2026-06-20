// NeuroBot — the "neuro interface": train the brain inside a NEURO block. Opened from
// the editor (tap a brain -> "train >"). The brain evolves to solve THIS level's maze;
// "Use it" saves the best brain back into the program's block. (engine: game/Evolve)
// Transfer learning: pick a saved brain as the "base" to fine-tune from, and save the
// fine-tuned result as an incremented version ("Brain v2") in your library.
#pragma once
#include <string>
#include <vector>
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Maze.h"
#include "game/Program.h"
#include "game/Evolve.h"
#include "game/Profile.h"

namespace screens {

class NeuroTrainScreen : public app::IScreen {
 public:
  void begin(gb::Profile* profile, gb::Program* prog, int brainIdx, gb::Maze* maze);
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;
  bool usedBrain() const { return _saved; }  // tapped "Use it" -> a brain was trained

 private:
  void draw();
  void mazeGeom(int& tile, int& ox, int& oy) const;
  void tracePath();
  void rebuildBrainLibs();  // library indices that carry a brain (loadable bases)
  void applyBase();         // load base _baseIdx into _brain and seed evolution
  int  nextVersion() const; // smallest free "Brain vN" number

  gb::Profile* _profile = nullptr;
  gb::Program* _prog = nullptr;
  int _idx = 0;
  gb::Maze* _maze = nullptr;
  gb::Evolve _evo;
  gb::Net _brain;         // the working brain (set by Teach=distill or Evolve)
  bool _taught = false;
  uint8_t _path[64];
  int _pathLen = 0;
  int _baseIdx = 0;            // 0 = this block's brain; >=1 = a library brain (transfer)
  std::string _baseName;       // display name of the current base
  std::vector<int> _brainLibs; // library indices that contain a brain
  bool _won = false, _saved = false, _savedCopy = false;
  app::TapDetector _tap;
};

}  // namespace screens
