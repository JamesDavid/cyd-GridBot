#include "screens/TuneNetLessonScreen.h"
#include "hal/Audio.h"

using namespace ui;
using namespace gb;

namespace screens {

// The "turn at a corner" task (same shape as the Hidden-layer lesson, i.e. XOR): turn when
// EXACTLY one side is open; go straight in a corridor (both walls) or a junction (both open).
static const float XIN[4][2]  = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
static const float YOUT[4][1] = {{0},    {1},    {1},    {0}};

// Learn-rate ladder: a few taps span tiny (crawls) -> tuned -> huge (thrashes), so the kid can
// actually FEEL both failure modes without 40 button presses.
static const float LR_LADDER[] = {0.05f, 0.10f, 0.20f, 0.35f, 0.50f, 0.80f, 1.20f, 1.80f, 2.50f};
static const int   LR_N = 9;

// knob steppers (two rows under the title)
static const Rect R_LR_DN  = {96,  26, 26, 20}, R_LR_UP  = {160, 26, 26, 20};
static const Rect R_RND_DN = {96,  50, 26, 20}, R_RND_UP = {160, 50, 26, 20};
// bottom bar
static const Rect R_TRAIN = {6,   (int16_t)(BOTBAR_Y + 2), 120, 26};
static const Rect R_RESET = {132, (int16_t)(BOTBAR_Y + 2), 80, 26};
static const Rect R_BACK  = {244, (int16_t)(BOTBAR_Y + 2), 70, 26};

static float lossOver(const Net& n) {           // mean squared error over the 4 examples
  float s = 0, out[NET_MAX_OUT];
  for (int i = 0; i < 4; i++) { n.forward(XIN[i], out); float d = out[0] - YOUT[i][0]; s += d * d; }
  return s / 4.0f;
}

float TuneNetLessonScreen::lr() const { return LR_LADDER[_lrIdx]; }

void TuneNetLessonScreen::begin() { _lrIdx = 4; _rounds = 2; _seed = 3; reset(); }

void TuneNetLessonScreen::reset() {
  _net.config(2, 4, 1, _seed);   // a real little MLP: 2 inputs -> 4 hidden -> 1 output
  _net.lr = lr();
  _lossLen = 0; _thrash = false;
  _last = lossOver(_net);
  pushLoss(_last);
}

void TuneNetLessonScreen::pushLoss(float l) {
  if (_lossLen < LOSS_N) _loss[_lossLen++] = l;
  else { for (int i = 1; i < LOSS_N; i++) _loss[i - 1] = _loss[i]; _loss[LOSS_N - 1] = l; }
}

void TuneNetLessonScreen::enter() { draw(); }

static void knob(LGFX& g, int y, const char* name, const char* val, const Rect& dn, const Rect& up,
                 uint16_t col) {
  label(g, 6, y + 4, name, col);
  button(g, dn, "-", C_INK, C_PANEL);
  label(g, 141, y + 4, val, C_ACCENT, textdatum_t::top_center);
  button(g, up, "+", C_INK, C_PANEL);
}

void TuneNetLessonScreen::draw() {
  auto& g = hal::display.gfx();
  g.fillScreen(C_BG);
  g.fillRect(0, 0, SCREEN_W, TOPBAR_H, C_PANEL);
  label(g, 6, 3, "Tune a real brain", C_ACCENT, textdatum_t::top_left, 2);
  char hd[20]; snprintf(hd, sizeof(hd), "loss %.3f", _last);
  label(g, SCREEN_W - 6, 6, hd, _last < 0.05f ? C_GO : C_DIM, textdatum_t::top_right);

  char v[10];
  snprintf(v, sizeof(v), "%.2f", lr());  knob(g, 24, "learn rate", v, R_LR_DN, R_LR_UP, C_MOVE);
  snprintf(v, sizeof(v), "%dx", _rounds); knob(g, 48, "rounds", v, R_RND_DN, R_RND_UP, C_LOOP);

  // loss curve: a real net's MSE falling (or bouncing) as it trains. High loss at the TOP, so a
  // smooth descent reads as a line sliding down to the floor -- "watch the loss fall".
  int gx = 196, gy = 24, gw = SCREEN_W - gx - 6, gh = 48;
  g.fillRoundRect(gx, gy, gw, gh, 3, C_PANEL);
  label(g, gx + 3, gy + 1, "loss", C_DIM);
  if (_lossLen >= 2) {
    int px = 0, py = 0;
    for (int i = 0; i < _lossLen; i++) {
      float vv = _loss[i]; if (vv > 0.5f) vv = 0.5f;     // clamp the y-scale (MSE rarely > 0.5)
      int cx = gx + 2 + (gw - 4) * i / (_lossLen - 1);
      int cy = gy + 4 + (int)((gh - 8) * (1.0f - vv / 0.5f));  // high loss at top -> line descends
      if (i > 0) g.drawLine(px, py, cx, cy, _thrash ? C_BAD : C_GO);
      px = cx; py = cy;
    }
  }

  // the 4 corner examples as ticks/crosses -- did the brain get each right?
  float out[NET_MAX_OUT];
  for (int i = 0; i < 4; i++) {
    _net.forward(XIN[i], out);
    bool ok = ((out[0] > 0.5f) == (YOUT[i][0] > 0.5f));
    int cx = 30 + i * 26, cy = 84;
    g.fillCircle(cx, cy, 9, ok ? C_GO : C_BAD);
    label(g, cx, cy - 4, ok ? "ok" : "x", C_BG, textdatum_t::top_center);
  }
  label(g, 140, 80, "the 4 corner cases", C_DIM);
  label(g, 6, 100, "same job as Hidden layer -- now tune HOW it learns", C_DIM);

  g.fillRect(0, BOTBAR_Y, SCREEN_W, BOTBAR_H, C_BG);
  button(g, R_TRAIN, "Train", C_GO, C_PANEL);
  button(g, R_RESET, "Reset", C_ACCENT, C_PANEL);
  button(g, R_BACK, "< Back", C_INK, C_PANEL);
  const char* msg;
  if (_last < 0.03f)       msg = "smooth descent to zero -- nicely tuned!";
  else if (_thrash)        msg = "learn rate too high -- loss won't settle!";
  else if (lr() < 0.15f)   msg = "barely moving -- turn learn rate UP";
  else                     msg = "press Train and watch the loss fall";
  label(g, SCREEN_W / 2, BOTBAR_Y - 10, msg,
        _last < 0.03f ? C_GO : (_thrash ? C_BAD : C_DIM), textdatum_t::bottom_center);
}

app::Signal TuneNetLessonScreen::tick(uint32_t now, const hal::TouchPoint& tp) {
  int tx, ty;
  if (!_tap.tapped(tp, now, tx, ty)) return app::Signal::NONE;
  if (R_TRAIN.contains(tx, ty)) {
    _net.lr = lr();
    float before = _last;
    for (int b = 0; b < _rounds; b++) {
      float l = 0;
      for (int e = 0; e < 50; e++) l = _net.trainEpoch(&XIN[0][0], &YOUT[0][0], 4);
      pushLoss(l);
    }
    _last = lossOver(_net);
    // "thrash" = too-high a learn rate: training made it WORSE, OR it's stuck high and won't settle
    // (very high lr parks XOR at the all-0.5 plateau, loss ~0.25, instead of converging).
    _thrash = (_last > before + 0.02f) || (lr() >= 1.2f && _last > 0.15f && _lossLen > 4);
    hal::audio.blip(); draw();
  } else if (R_LR_DN.contains(tx, ty)) { if (_lrIdx > 0) _lrIdx--; reset(); hal::audio.blip(); draw(); }
  else if (R_LR_UP.contains(tx, ty))   { if (_lrIdx < LR_N - 1) _lrIdx++; reset(); hal::audio.blip(); draw(); }
  else if (R_RND_DN.contains(tx, ty))  { if (_rounds > 1) _rounds--; reset(); hal::audio.blip(); draw(); }
  else if (R_RND_UP.contains(tx, ty))  { if (_rounds < 6) _rounds++; reset(); hal::audio.blip(); draw(); }
  else if (R_RESET.contains(tx, ty))   { _seed = _seed * 1664525u + 1013904223u; reset(); hal::audio.blip(); draw(); }
  else if (R_BACK.contains(tx, ty))    return app::Signal::BACK;
  return app::Signal::NONE;
}

}  // namespace screens
