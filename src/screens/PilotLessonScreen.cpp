#include "screens/PilotLessonScreen.h"
#include "hal/Audio.h"
#include "assets/Assets.h"
#include "game/MazeGen.h"
#include "game/Program.h"
#include "game/Interpreter.h"
#include "game/Pilot.h"

using namespace ui;
using namespace gb;

namespace screens {

static const uint32_t DEMO_SEED = 4242u;

static const Rect R_NEXT = {6,   (int16_t)(BOTBAR_Y + 2), 188, 26};
static const Rect R_BACK = {200, (int16_t)(BOTBAR_Y + 2), 114, 26};

static const char* STATUS[3] = {
  "Brain alone: it only senses nearby - it gets stuck.",
  "Planner adds waypoints (dots). Brain just steers dot-to-dot -> solved!",
  "Tesla FSD is the same: a MAP plans the route, the brain handles the road.",
};
static const char* BTN[3] = {"Add a route planner >", "Why does this work? >", "Start over"};

void PilotLessonScreen::begin() {
  _brain.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 1);
  pilotTrain(_brain, DEMO_SEED, 14, 18);   // a competent waypoint-follower
  // Pick a demo maze where the brain ALONE fails but PILOTED it wins (the clearest contrast).
  for (int lvl = 5; lvl <= 14; lvl++) {
    MazeGen::generate(_maze, DEMO_SEED, lvl);
    trace(false); bool solo = _won;
    trace(true);  bool pilot = _won;
    if (!solo && pilot) break;
  }
  _phase = 0;
  trace(false);
}

void PilotLessonScreen::enter() { draw(); }

void PilotLessonScreen::trace(bool pilot) {
  Program prog; prog.brains.push_back(_brain);
  Node loop = Node::repeatUntil(AT_GOAL);
  loop.body.push_back(pilot ? Node::pilotBrain(0) : Node::neuro(0));
  prog.main.push_back(loop);
  Interpreter it; it.load(&prog, &_maze, _maze.startPose(), 200);
  _pathLen = 0; _won = false;
  for (int s = 0; s < 200; s++) {
    Pose p = it.pose();
    if (_pathLen < (int)sizeof(_path) && (_pathLen == 0 ||
        _path[_pathLen - 1] != (uint8_t)(p.row * _maze.cols() + p.col)))
      _path[_pathLen++] = (uint8_t)(p.row * _maze.cols() + p.col);
    if (it.finished()) { _won = (it.lastOutcome() == OUT_WIN); break; }
    it.step();
  }
  _wpN = pilot ? planWaypoints(_maze, _maze.startPose(), _wp, 40) : 0;
}

void PilotLessonScreen::mazeGeom(int& tile, int& ox, int& oy) const {
  int cols = _maze.cols(), rows = _maze.rows();
  tile = (SCREEN_W - 20) / cols; int th = (BAND_H - 40) / rows;
  if (th < tile) tile = th; if (tile > 28) tile = 28;
  ox = (SCREEN_W - tile * cols) / 2; oy = BAND_Y + 30;
}

void PilotLessonScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Pilot: plan + steer", C_ACCENT, textdatum_t::top_left, 2);
  label(g, 6, TOPBAR_H + 4, STATUS[_phase], _won ? C_GO : C_INK);

  if (_phase == 2) {
    // the analogy, spelled out
    int y = BAND_Y + 26;
    label(g, 14, y,      "PLANNER  -  decides WHERE to go", ui::rgb(120, 230, 245)); y += 22;
    label(g, 22, y,      "route / waypoints (the dots)", C_DIM); y += 26;
    label(g, 14, y,      "BRAIN    -  decides HOW to get there", C_ACCENT); y += 22;
    label(g, 22, y,      "steer dot-to-dot, jump pits, round walls", C_DIM); y += 28;
    label(g, 14, y,      "Neither solves the maze alone.", C_INK); y += 18;
    label(g, 14, y,      "Together they clear the whole campaign.", C_GO);
  } else {
    int tile, ox, oy; mazeGeom(tile, ox, oy);
    for (int r = 0; r < _maze.rows(); r++)
      for (int c = 0; c < _maze.cols(); c++) {
        int x = ox + c * tile, y = oy + r * tile;
        Tile t = _maze.at(r, c);
        uint16_t col = ((r + c) & 1) ? C_FLOOR : C_FLOOR2;
        if (t == WALL) col = C_WALL; else if (t == PIT) col = C_BG;
        g.fillRect(x, y, tile - 1, tile - 1, col);
        if (_maze.isGoal(r, c)) assets::drawGoalToken(g, x + tile / 2, y + tile / 2, tile, 0);
      }
    // planner waypoints (phase 1): hollow yellow rings the brain steers between
    for (int i = 0; i < _wpN; i++) {
      int r = _wp[i] / _maze.cols(), c = _wp[i] % _maze.cols();
      g.drawCircle(ox + c * tile + tile / 2, oy + r * tile + tile / 2, tile / 3, C_ACCENT);
    }
    // the brain's actual path
    for (int i = 0; i < _pathLen; i++) {
      int r = _path[i] / _maze.cols(), c = _path[i] % _maze.cols();
      g.fillCircle(ox + c * tile + tile / 2, oy + r * tile + tile / 2, tile / 6 + 1,
                   _won ? C_GO : C_BAD);
    }
  }

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_NEXT, BTN[_phase], C_GO, C_PANEL);
  button(g, R_BACK, "Back", C_INK, C_PANEL);
}

app::Signal PilotLessonScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  if (R_NEXT.contains(tx, ty)) {
    if (_phase == 0)      { _phase = 1; trace(true); }
    else if (_phase == 1) { _phase = 2; }
    else                  { _phase = 0; trace(false); }
    hal::audio.blip(); draw();
  } else if (R_BACK.contains(tx, ty)) {
    return app::Signal::BACK;
  }
  return app::Signal::NONE;
}

}  // namespace screens
