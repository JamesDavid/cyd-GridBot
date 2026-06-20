#include "screens/CodeLessonScreen.h"
#include "hal/Audio.h"
#include "assets/Assets.h"
#include "game/MazeGen.h"
#include "screens/ProgramEditor.h"

using namespace ui;
using namespace gb;

namespace screens {

static const Rect R_RUN  = {6,   (int16_t)(BOTBAR_Y + 2), 100, 26};
static const Rect R_RST  = {112, (int16_t)(BOTBAR_Y + 2), 90, 26};
static const Rect R_BACK = {244, (int16_t)(BOTBAR_Y + 2), 70, 26};
static constexpr int PROG_X = 168;

static Pose pose(int r, int c, Facing f) { Pose p; p.row = (int8_t)r; p.col = (int8_t)c; p.facing = f; return p; }

void CodeLessonScreen::setup(int lesson) {
  _prog.clear();
  switch (lesson) {
    case 0:  // Move
      _title = "Move"; _concept = "Forward steps the way the robot faces. Turn spins it.";
      _maze.reset(2, 3); _maze.fill(FLOOR); _maze.setStart(pose(1, 0, EAST)); _maze.setGoal(0, 2);
      _prog.main.push_back(Node::command(CMD_FWD));
      _prog.main.push_back(Node::command(CMD_FWD));
      _prog.main.push_back(Node::command(CMD_TURN_L));
      _prog.main.push_back(Node::command(CMD_FWD));
      break;
    case 2:  // Repeat
      _title = "Repeat"; _concept = "A repeat loop runs the steps inside it again and again.";
      _maze.reset(1, 6); _maze.fill(FLOOR); _maze.setStart(pose(0, 0, EAST)); _maze.setGoal(0, 5);
      { Node rep = Node::repeat(5); rep.body.push_back(Node::command(CMD_FWD)); _prog.main.push_back(rep); }
      break;
    default:  // Sense / If (case 4 — last to unlock)
      _title = "Sense"; _concept = "IF lets the robot react: if a wall is ahead, turn.";
      MazeGen::generateSpiral(_maze, 5, 5);
      { Node loop = Node::repeatUntil(AT_GOAL);
        Node iff = Node::ifCond(WALL_AHEAD); iff.body.push_back(Node::command(CMD_TURN_L));
        loop.body.push_back(iff); loop.body.push_back(Node::command(CMD_FWD));
        _prog.main.push_back(loop); }
      break;
    case 3:  // Functions
      _title = "Functions"; _concept = "A function is steps you name once and CALL many times.";
      _maze.reset(1, 5); _maze.fill(FLOOR); _maze.setStart(pose(0, 0, EAST)); _maze.setGoal(0, 4);
      _prog.f1.push_back(Node::command(CMD_FWD));
      for (int i = 0; i < 4; i++) _prog.main.push_back(Node::call(1));
      break;
    case 1:  // Jump (unlocks at level 6, before Repeat)
      _title = "Jump"; _concept = "Jump leaps over a single pit to the tile beyond.";
      _maze.reset(1, 5); _maze.fill(FLOOR); _maze.set(0, 2, PIT);
      _maze.setStart(pose(0, 0, EAST)); _maze.setGoal(0, 4);
      _prog.main.push_back(Node::command(CMD_FWD));
      _prog.main.push_back(Node::command(CMD_JUMP));
      _prog.main.push_back(Node::command(CMD_FWD));
      break;
  }
  _it.load(&_prog, &_maze, _maze.startPose());
  _drawn = _maze.startPose();
  _running = false; _done = false;
}

void CodeLessonScreen::begin(int lesson) { _lesson = lesson; setup(lesson); }
void CodeLessonScreen::enter() { draw(); }

void CodeLessonScreen::mazeGeom(int& tile, int& ox, int& oy) const {
  int cols = _maze.cols(), rows = _maze.rows();
  tile = (PROG_X - 16) / cols; int th = (BAND_H - 30) / rows;
  if (th < tile) tile = th; if (tile > 34) tile = 34;
  ox = 8; oy = BAND_Y + 36;
}

void CodeLessonScreen::drawRobot() {
  auto& g = hal::display.gfx();
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
  Pose p = _it.pose();
  assets::drawCharacter(g, ox + p.col * tile + tile / 2, oy + p.row * tile + tile / 2, tile, 0, p.facing);
}

void CodeLessonScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  char t[24]; snprintf(t, sizeof(t), "Code: %s", _title);
  label(g, 6, 3, t, C_ACCENT, textdatum_t::top_left, 2);
  // wrap the concept onto the line under the bar (it's short)
  label(g, 6, TOPBAR_H + 4, _concept, C_INK);

  drawRobot();

  // the program, rendered in the SAME row style as the real editor (glyph + colour +
  // indented bracket) so the lessons match the game's UI (not a separate "code look")
  g.drawFastVLine(PROG_X - 4, BAND_Y + 18, BAND_H - 20, C_PANEL_HI);
  label(g, PROG_X + 6, BAND_Y + 20, "program:", C_DIM);
  int py = BAND_Y + 36;
  if (!_prog.f1.empty()) {
    label(g, PROG_X + 6, py, "F1:", C_FUNC); py += 16;
    py = ProgramEditor::drawReadOnlyList(g, _prog.f1, PROG_X + 8, py);
    py += 4;
  }
  ProgramEditor::drawReadOnlyList(g, _prog.main, PROG_X + 2, py);

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_RUN, _running ? "..." : "Run >", C_GO, C_PANEL);
  button(g, R_RST, "Reset", C_ACCENT, C_PANEL);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
  if (_done) {
    bool won = _it.lastOutcome() == OUT_WIN;
    label(g, 6, BOTBAR_Y - 12, won ? "solved it!" : "hmm, try Run again", won ? C_GO : C_BAD);
  }
}

app::Signal CodeLessonScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  if (_running && !_it.finished() && now - _last >= 360) {
    _last = now;
    _it.step();
    drawRobot();
    if (_it.finished()) {
      _running = false; _done = true;
      hal::audio.win();
      draw();
    }
  }
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  if (R_RUN.contains(tx, ty)) {
    _it.load(&_prog, &_maze, _maze.startPose());
    _running = true; _done = false; _last = now; draw();
  } else if (R_RST.contains(tx, ty)) {
    _it.load(&_prog, &_maze, _maze.startPose());
    _running = false; _done = false; draw();
  } else if (R_BACK.contains(tx, ty)) {
    return app::Signal::BACK;
  }
  return app::Signal::NONE;
}

}  // namespace screens
