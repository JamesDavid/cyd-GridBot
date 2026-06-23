// CodeLab lesson — teach one core programming block with a tiny runnable demo. Each
// lesson sets up a small maze + a pre-built program that uses the block; tap Run and watch
// the robot solve it. Separate from NeuroLab (which teaches ML). See CodeLabScreen for the
// list. Lessons: 0 Move, 1 Jump, 2 Repeat, 3 Sense/If, 4 Functions, 5 Brain (neurosymbolic),
// 6 Debug (a broken program + a one-tap Fix it).
#pragma once
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Maze.h"
#include "game/Program.h"
#include "game/Interpreter.h"

namespace screens {

class CodeLessonScreen : public app::IScreen {
 public:
  void begin(int lesson);
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

 private:
  void setup(int lesson);
  void draw();
  void mazeGeom(int& tile, int& ox, int& oy) const;
  void drawRobot();

  int _lesson = 0;
  const char* _title = "";
  const char* _concept = "";
  gb::Maze _maze;
  gb::Program _prog;
  gb::Interpreter _it;
  bool _running = false;
  bool _done = false;
  bool _isDebug = false;   // the Debug lesson shows a "Fix it" button instead of Reset
  bool _fixed = false;     // has the kid applied the one-line fix yet?
  uint32_t _last = 0;
  gb::Pose _drawn;
  app::TapDetector _tap;
};

}  // namespace screens
