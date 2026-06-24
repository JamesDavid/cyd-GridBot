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
  void drawLockModal();   // "<feature> is locked -> unlock at Lv N / learn it in the <lesson>"
  int _lockInfo = 0;      // 0 none; 1 Draw, 2 Evolve, 3 Q-Learn, 4 Pilot (a locked button was tapped)
  void mazeGeom(int& tile, int& ox, int& oy) const;
  void tracePath();
  void rebuildBrainLibs();  // library indices that carry a brain (loadable bases)
  void applyBase();         // load base _baseIdx into _brain and seed evolution
  int  nextVersion() const; // smallest free "Brain vN" number
  void seedDrawStart();     // begin a drawn path at the robot's start tile
  bool tileAtPixel(int x, int y, int& r, int& c) const;  // maze hit-test
  void handleDrawTap(int r, int c);  // append/undo a tile on the drawn path
  void setNodePilot(bool on);  // flag this brain's N_NEURO node as a pilot (planner+follower)
  void setNodeRnn(bool on);    // flag this brain's N_NEURO node as recurrent (memory)

  gb::Profile* _profile = nullptr;
  gb::Program* _prog = nullptr;
  int _idx = 0;
  gb::Maze* _maze = nullptr;
  gb::Evolve _evo;
  gb::Net _brain;         // the working feedforward brain (Teach/Draw/Evolve/Generalize/Pilot)
  gb::RNet _rbrain;       // the working recurrent brain (rnn mode)
  bool _taught = false;
  uint8_t _path[64];
  int _pathLen = 0;
  int _baseIdx = 0;            // 0 = this block's brain; >=1 = a library brain (transfer)
  std::string _baseName;       // display name of the current base
  std::vector<int> _brainLibs; // library indices that contain a brain
  bool _won = false, _saved = false, _savedCopy = false;
  bool _drawMode = false;      // kid is hand-drawing the path to learn from
  uint8_t _drawPath[64];       // ordered tile indices of the drawn route
  int _drawLen = 0;
  bool _wpMode = false;        // kid is placing PILOT waypoints (their own route for the follower)
  uint8_t _wpPath[40];         // ordered waypoint tile indices
  int _wpLen = 0;
  void handleWpTap(int r, int c);  // drop/undo a waypoint
  void enterWaypointMode();        // open the route-placing screen (loads any existing waypoints)
  int _gauntletScore = -1;     // last Generalist-challenge result (levels cleared), -1 = not run
  bool _pilotMode = false;     // preview/save the brain as a planner-fed pilot
  bool _rnnMode = false;       // this brain block is recurrent (memory) — train/run the RNet
  // a tiny always-on "neuron activity" widget in the top bar: input->hidden->output dots
  // light in a wave so the page never looks frozen; it pulses brighter just after training.
  uint32_t _widgetAt = 0;      // last widget-frame time (throttle)
  uint32_t _pulseUntil = 0;    // pulse brightly/faster until this time (set on a train action)
  void drawNeuronWidget(uint32_t now);
  // tap the widget to expand the full Brain-Cam network graph of THIS brain (and back)
  bool _brainView = false;
  float _in[gb::SENSOR_COUNT_FOR_BRAIN] = {0};
  float _hid[gb::NET_MAX_HID] = {0};
  float _out[gb::NET_MAX_OUT] = {0};
  int   _action = 0;
  void inferBrain();           // activations of the working brain at the maze start
  app::TapDetector _tap;
};

}  // namespace screens
