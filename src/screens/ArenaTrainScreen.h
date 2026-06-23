// NeuroBot — Arena Trainer: prep a brain to BATTLE. It evolves (or is taught) to beat an
// AI opponent on a real arena board; "Save" drops the trained fighter into your library so
// it shows up as an Arena opponent you can pick. (engines: Evolve / Distill)
#pragma once
#include <string>
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Maze.h"
#include "game/Program.h"
#include "game/Evolve.h"
#include "game/Profile.h"

namespace screens {

class ArenaTrainScreen : public app::IScreen {
 public:
  void begin(gb::Profile* profile);
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;
  bool savedFighter() const { return _saved; }  // tapped "Save fighter"
  // "Fight! >" was tapped: the fighter is saved and the app should jump straight into a battle of
  // this fighter vs the current sparring opponent (instead of returning to the Arena menu).
  bool launchFight() const { return _launchFight; }
  int  fightLibIdx() const { return _savedIdx; }       // library slot of the fighter to field
  const char* fightOppName() const { return _oppName.c_str(); }
  bool fightSumo() const { return _matchType == gb::MatchType::SUMO; }

 private:
  void draw();
  void mazeGeom(int& tile, int& ox, int& oy) const;
  void evaluateAndTrace();
  int  oppCount() const;        // sparring roster size (house bots + your library)
  void buildOpponent(int idx);  // load opponent `idx` into _ai (+ name into _oppName)
  void setSelfOpponent(const gb::Net& b);  // _ai := a copy of brain `b` (self-play sparring partner)
  std::string oppNameFor(int idx) const;  // display name for a roster slot (no side effects)
  void drawOppList();           // the tap-to-pick opponent menu (replaces cycling)
  ui::Rect oppRowRect(int i) const;
  int  saveFighterToLibrary();  // persist the current brain as a fighter; returns its library index
  bool _oppPick = false;        // showing the opponent picker overlay
  bool _launchFight = false;    // Back was used to jump straight into a battle
  void setupBoard();            // (re)generate the arena; Sumo clears the goal (last-standing)
  void setMode(gb::MatchType t);// switch Race<->Sumo: reboard, rebuild opponent, restart evo

  gb::Profile* _profile = nullptr;
  gb::Maze _maze;
  gb::Pose _s0, _s1;
  gb::Program _ai;       // the current sparring opponent (cyclable: AI / house / library)
  gb::Evolve _evo;
  gb::Net _brain;
  // The recurrent (memory) brain, trained instead of _brain when _rnn is on. Heap-allocated (it's
  // ~1.5KB and the screen's static DRAM is nearly full); created on first use via rbrain().
  gb::RNet* _rbrain = nullptr;
  gb::RNet& rbrain();    // lazily allocate + return the RNN brain
  bool _rnn = false;     // brain-type toggle: train/save/show the RNN brain rather than feedforward
  uint8_t _path[64];
  int _pathLen = 0;
  uint8_t _oppPath[64];      // the opponent's route when it runs ITS code (shown in red)
  int _oppLen = 0;
  gb::MatchType _matchType = gb::MatchType::RACE;  // Race (to goal) or Sumo (last bot standing)
  int _oppIdx = 0;            // which roster opponent we spar against
  std::string _oppName;      // its display name
  bool _beatsAI = false, _taught = false, _saved = false;
  int _savedIdx = -1;         // library slot this session's fighter went to (-1 = not saved yet)
  std::string nextFighterName() const;  // smallest free "Fighter vN" so each is distinct
  // first-ever battle bot (empty library, nothing saved this session) -> show the onboarding hint
  bool firstFighter() const { return _profile && _profile->library.empty() && _savedIdx < 0; }

  // "watch it learn" view: the Brain-Cam network graph + an arena mini-map (you vs the
  // opponent), so a kid sees the opponent, their path, AND the network all update together.
  bool _netView = false;
  bool _animating = false;    // Evolve animates generation-by-generation
  bool _qLearning = false;    // the current animation is Q-Learn (reward) rather than Evolve (selection)
  int  _qChunks = 8;          // Q-Learn chunks this run (8 = FF battle hunt; 4 = RNN race maze)
  int  _animLeft = 0;         // generations left in the current animation
  uint32_t _animAt = 0;       // last hunt-frame time (throttle)
  uint32_t _genAt = 0;        // last generation-step time (evolves in the background)
  int  _animFrame = 0;        // how many steps of the hunt to reveal (sped-up live playback)
  float _in[gb::SENSOR_COUNT_FOR_BRAIN] = {0};
  float _hid[gb::NET_MAX_HID] = {0};
  float _out[gb::NET_MAX_OUT] = {0};
  int   _action = 0;
  void inferBrain();          // activations of the working brain at its start pose
  void drawNet();             // network graph + arena mini-map + status
  // Learning curve: how good the brain is (0..1) sampled each training step, so a kid (or an ML
  // engineer over a beer) sees fitness CLIMB as it learns -- the metric overlay the Brain Cam wanted.
  static constexpr int CURVE_N = 48;
  float _curve[CURVE_N] = {0};
  int   _curveLen = 0;
  float _score = 0.0f;        // current brain's score vs the opponent (HP margin / goal progress)
  void  pushCurve();          // append _score to the curve (during an animated training run)
  void  drawCurve(int x, int y, int w, int h);
  app::TapDetector _tap;
};

}  // namespace screens
