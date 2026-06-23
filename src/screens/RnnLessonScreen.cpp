#include "screens/RnnLessonScreen.h"
#include "hal/Audio.h"
#include "assets/Assets.h"
#include "game/MazeGen.h"
#include "game/Sensors.h"
#include "game/Reactive.h"

using namespace ui;
using namespace gb;

namespace screens {

static const uint32_t DEMO_SEED = 4242u;

static const Rect R_NEXT = {6,   (int16_t)(BOTBAR_Y + 2), 188, 26};
static const Rect R_BACK = {200, (int16_t)(BOTBAR_Y + 2), 114, 26};

static const char* STATUS[4] = {
  "No memory: it forgets it's been here - loops.",
  "Memory (RNN): remembers its trail -> solved!",
  "New: the THINK layer feeds back into itself (8x8).",
  "RNN action = senses NOW + memory (remembers).",
};
static const char* BTN[4] = {"Now give it memory >", "See the one new wire >",
                             "Why it works >", "Start over"};

// One explorer episode -> (senses, action) examples (the memory-using teacher).
static int rollout(const Maze& m, float* X, int* act, int maxT) {
  uint8_t vis[MAZE_MAX_CELLS] = {0};
  Pose p = m.startPose(); vis[p.row * m.cols() + p.col] = 1;
  int T = 0;
  for (int s = 0; s < maxT; s++) {
    if (m.isGoal(p.row, p.col)) break;
    senseEgo(m, p, nullptr, X + T * SENSOR_COUNT);
    Cmd c = exploreActionTo(m, p, vis, m.goalRow(), m.goalCol());
    act[T] = reactiveCmdToAction(c); T++;
    if (reactiveApply(m, p, c) != OUT_OK) break;
    vis[p.row * m.cols() + p.col] = 1;
  }
  return T;
}

void RnnLessonScreen::begin() {
  // Same 8-neuron hidden layer as the real robot brain; the ONLY difference is the RNN's
  // recurrent memory loop, so the kid sees memory (not extra neurons) do the work.
  _ff.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 1);
  _rnn.config(SENSOR_COUNT_FOR_BRAIN, 8, 5, 1); _rnn.lr = 0.05f;
  static float X[56 * SENSOR_COUNT]; static int act[56];
  for (int e = 0; e < 60; e++)
    for (int l = 1; l <= 26; l++) {
      Maze m; MazeGen::generate(m, DEMO_SEED, l);
      int T = rollout(m, X, act, 56);
      if (T <= 0) continue;
      _rnn.trainEpisode(X, act, T);
      for (int t = 0; t < T; t++) { float tg[NET_MAX_OUT] = {0}; tg[act[t]] = 1.0f; _ff.trainStep(X + t * SENSOR_COUNT, tg); }
    }
  // demo maze: the brain WITHOUT memory fails, WITH memory wins (clearest contrast)
  for (int l = 6; l <= 30; l++) {
    MazeGen::generate(_maze, DEMO_SEED, l);
    trace(false); bool ff = _won;
    trace(true);  bool rn = _won;
    if (!ff && rn) break;
  }
  _phase = 0;
  trace(false);
}

void RnnLessonScreen::enter() { draw(); }

void RnnLessonScreen::trace(bool rnn) {
  Pose p = _maze.startPose();
  if (rnn) _rnn.resetState();
  _pathLen = 0; _won = false;
  _path[_pathLen++] = (uint8_t)(p.row * _maze.cols() + p.col);
  for (int s = 0; s < 250; s++) {
    if (_maze.isGoal(p.row, p.col)) { _won = true; break; }
    float sv[SENSOR_COUNT]; senseEgo(_maze, p, nullptr, sv);
    int a = rnn ? _rnn.argmaxStep(sv) : _ff.argmax(sv);
    Cmd c = a == 1 ? CMD_TURN_L : a == 2 ? CMD_TURN_R : a == 3 ? CMD_JUMP : CMD_FWD;
    Outcome o = reactiveApply(_maze, p, c);
    if (o == OUT_BONK || o == OUT_FELL) break;
    uint8_t t = (uint8_t)(p.row * _maze.cols() + p.col);
    if (_pathLen < (int)sizeof(_path) && _path[_pathLen - 1] != t) _path[_pathLen++] = t;
    if (o == OUT_WIN) { _won = true; break; }
  }
}

void RnnLessonScreen::mazeGeom(int& tile, int& ox, int& oy) const {
  int cols = _maze.cols(), rows = _maze.rows();
  tile = (SCREEN_W - 20) / cols; int th = (BAND_H - 40) / rows;
  if (th < tile) tile = th; if (tile > 28) tile = 28;
  ox = (SCREEN_W - tile * cols) / 2; oy = BAND_Y + 30;
}

void RnnLessonScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Memory: the RNN", C_ACCENT, textdatum_t::top_left, 2);
  label(g, 6, TOPBAR_H + 4, STATUS[_phase], _won ? C_GO : C_INK);

  if (_phase == 2) {
    // "See the one new wire": the 10->8->5 brain with the recurrent memory loop drawn in.
    const int IX = 50, HX = 150, OX = 252;
    const uint16_t cyan = ui::rgb(120, 230, 245);
    label(g, IX - 18, 40, "SEES", C_MOVE);
    label(g, HX - 22, 40, "THINKS", C_SENSE);
    label(g, OX - 18, 40, "DOES", C_GO);
    int iy[4], hy[8], oy2[5];
    for (int i = 0; i < 4; i++) iy[i] = 54 + i * 26;
    for (int j = 0; j < 8; j++) hy[j] = 50 + j * 12;
    for (int k = 0; k < 5; k++) oy2[k] = 52 + k * 21;
    // dim forward wires (the brain it always had)
    for (int i = 0; i < 4; i++) for (int j = 0; j < 8; j++) g.drawLine(IX, iy[i], HX, hy[j], C_LOCK);
    for (int j = 0; j < 8; j++) for (int k = 0; k < 5; k++) g.drawLine(HX, hy[j], OX, oy2[k], C_LOCK);
    // THE memory feedback: the WHOLE hidden layer feeds back into the whole hidden layer
    // (an 8x8 weight matrix, not one wire). Drawn as a bundle of arcs to read as "many".
    for (int strand = 0; strand < 3; strand++) {
      int b = 18 + strand * 9;
      int ax[5] = {HX, HX + b * 3 / 4, HX + b, HX + b * 3 / 4, HX};
      int ay[5] = {hy[0], 70, 91, 112, hy[7]};
      for (int s = 0; s < 4; s++) g.drawLine(ax[s], ay[s], ax[s + 1], ay[s + 1], cyan);
    }
    g.fillTriangle(HX, hy[7], HX + 9, hy[7] - 5, HX + 9, hy[7] + 4, cyan);  // arrowhead into column
    label(g, HX + 42, 86, "8x8", cyan);
    // nodes on top
    for (int i = 0; i < 4; i++) g.fillCircle(IX, iy[i], 4, C_MOVE);
    for (int j = 0; j < 8; j++) g.fillCircle(HX, hy[j], 5, C_SENSE);
    for (int k = 0; k < 5; k++) g.fillCircle(OX, oy2[k], 5, C_GO);
    // the takeaway (the kid's own framing: the feedback weights were 0, now they're trained)
    int y = 150;
    label(g, 10, y, "Same 10 -> 8 -> 5 brain + a feedback path:", C_INK); y += 14;
    label(g, 10, y, "every THINK neuron -> every THINK neuron", cyan); y += 14;
    label(g, 10, y, "(8x8 = 64 wires, including itself).", C_DIM); y += 14;
    label(g, 10, y, "A plain brain has them 0; the RNN trains them.", C_GO);
  } else if (_phase == 3) {
    int y = BAND_Y + 26;
    label(g, 14, y, "FEEDFORWARD brain", C_MOVE); y += 18;
    label(g, 22, y, "action = f(senses NOW)", C_DIM); y += 16;
    label(g, 22, y, "same view -> same move -> it can loop", C_DIM); y += 22;
    label(g, 14, y, "RECURRENT brain (RNN)", C_ACCENT); y += 18;
    label(g, 22, y, "action = f(senses NOW + memory)", C_DIM); y += 16;
    label(g, 22, y, "remembers its trail -> escapes dead-ends", C_DIM); y += 22;
    label(g, 14, y, "Pilot used an OUTSIDE planner; the RNN", C_INK); y += 16;
    label(g, 14, y, "grows the memory INSIDE the brain.", C_GO); y += 18;
    label(g, 14, y, "real AIs: LSTM & Transformers (chatbots).", C_DIM);
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
    for (int i = 0; i < _pathLen; i++) {
      int r = _path[i] / _maze.cols(), c = _path[i] % _maze.cols();
      g.fillCircle(ox + c * tile + tile / 2, oy + r * tile + tile / 2, tile / 6 + 1, _won ? C_GO : C_BAD);
    }
  }

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_NEXT, BTN[_phase], C_GO, C_PANEL);
  button(g, R_BACK, "Back", C_INK, C_PANEL);
}

app::Signal RnnLessonScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  if (R_NEXT.contains(tx, ty)) {
    if (_phase == 0)      { _phase = 1; trace(true); }   // give it memory -> solves
    else if (_phase == 1) { _phase = 2; }                // see the loop diagram
    else if (_phase == 2) { _phase = 3; }                // the takeaway
    else                  { _phase = 0; trace(false); }  // start over
    hal::audio.blip(); draw();
  } else if (R_BACK.contains(tx, ty)) {
    return app::Signal::BACK;
  }
  return app::Signal::NONE;
}

}  // namespace screens
