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

 private:
  void draw();
  void mazeGeom(int& tile, int& ox, int& oy) const;
  void evaluateAndTrace();
  int  oppCount() const;        // sparring roster size (house bots + your library)
  void buildOpponent(int idx);  // load opponent `idx` into _ai (+ name into _oppName)
  void setupBoard();            // (re)generate the arena; Sumo clears the goal (last-standing)
  void setMode(gb::MatchType t);// switch Race<->Sumo: reboard, rebuild opponent, restart evo

  gb::Profile* _profile = nullptr;
  gb::Maze _maze;
  gb::Pose _s0, _s1;
  gb::Program _ai;       // the current sparring opponent (cyclable: AI / house / library)
  gb::Evolve _evo;
  gb::Net _brain;
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

  // "watch it learn" view: the Brain-Cam network graph + an arena mini-map (you vs the
  // opponent), so a kid sees the opponent, their path, AND the network all update together.
  bool _netView = false;
  bool _animating = false;    // Evolve animates generation-by-generation
  int  _animLeft = 0;         // generations left in the current animation
  uint32_t _animAt = 0;       // last animation-step time (throttle)
  float _in[gb::SENSOR_COUNT_FOR_BRAIN] = {0};
  float _hid[gb::NET_MAX_HID] = {0};
  float _out[gb::NET_MAX_OUT] = {0};
  int   _action = 0;
  void inferBrain();          // activations of the working brain at its start pose
  void drawNet();             // network graph + arena mini-map + status
  app::TapDetector _tap;
};

}  // namespace screens
