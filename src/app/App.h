// GridBot — top-level app / state machine (SPEC §3, §11).
// PROFILE_SELECT -> PROFILE_CREATE / LEVEL_INTRO -> GAME -> (WIN) -> LEVEL_INTRO ; STATS.
#pragma once
#include <stdint.h>
#include "app/Screen.h"
#include "game/Profile.h"
#include "screens/GameScreen.h"
#include "screens/HomeScreen.h"
#include "screens/LevelSelectScreen.h"
#include "screens/ProfileSelectScreen.h"
#include "screens/ProfileCreateScreen.h"
#include "screens/StatsScreen.h"
#include "screens/ArenaScreen.h"
#include "screens/RadioScreen.h"
#include "screens/PixelEditorScreen.h"
#include "screens/BadgesScreen.h"
#include "screens/ShopScreen.h"
#include "screens/PuzzleRaceScreen.h"
#include "screens/ChallengeScreen.h"
#include "screens/NeuroLessonScreen.h"
#include "screens/LessonHubScreen.h"
#include "screens/QLessonScreen.h"
#include "screens/TuneLessonScreen.h"
#include "screens/TuneNetLessonScreen.h"
#include "screens/SelfPlayLessonScreen.h"
#include "screens/MethodLessonScreen.h"
#include "screens/EvoLessonScreen.h"
#include "screens/NeuroTrainScreen.h"
#include "screens/ArenaTrainScreen.h"
#include "screens/LessonsMenuScreen.h"
#include "screens/CodeLabScreen.h"
#include "screens/CodeLessonScreen.h"
#include "screens/TransferLessonScreen.h"
#include "screens/PilotLessonScreen.h"
#include "screens/RnnLessonScreen.h"
#include "screens/PerceptionLessonScreen.h"
#include "screens/BackpropLessonScreen.h"
#include "screens/LibraryScreen.h"
#include "screens/BrainViewScreen.h"
#include "screens/BrainMapScreen.h"

namespace app {

class App {
 public:
  void begin();
  void drawSplash();   // boot splash: title + firmware version + repo URL, shown briefly
  void tick(uint32_t now);
  void debugGoToLevel(uint32_t level);    // playtest aid (serial 'G <n>')
  void debugFastPlay(uint32_t target);    // auto-solve+win levels up to target ('P <n>')
  void debugHome();                       // force the profile-select screen ('H')
  void debugGrantCoins(uint32_t n);       // grant coins for testing ('C <n>')
  void debugAutoRun();                    // GAME: solve + run paused ('A')
  void debugStep();                       // advance the current run/match one tick ('N')
  void debugNeuroLesson();                // open the single-neuron lesson ('B')
  void debugDumpMaze();                   // GAME: print the exact maze grid over serial ('M')
  void debugLoadProg(uint32_t n);         // GAME: load a hand-coding-guide program into the editor ('D <n>')
  void debugZapDemo();                    // field a hand-coded zap-swap soccer bot vs Strika ('K')
  void debugSwapDemo();                   // controlled zap-swap scenario for the demo gif ('U')
  void debugSoccerDemo();                 // controlled clean-dribble soccer scenario for the gif ('O')
  void debugFightLib(int a, int b);       // list the library ('F'), or field lib[a] vs lib[b] in soccer ('F a b')
  void debugLevelRecs(int lvl, int stars); // dump per-level records ('R'); 'R lvl stars' force-records one
  void debugBrain(int idx, const char* hex);  // 'J' lists brains; 'J i' dumps lib[i]'s weights as hex; 'J i <hex>' loads
  void debugEval(int a, int b, int seeds, int type); // 'V a b n t' plays lib[a] vs lib[b] over n seeds (repro hashes)
  void debugStats();                       // 'I' prints heap / profile stats

 private:
  enum class State : uint8_t { SELECT, CREATE, HOME, INTRO, GAME, STATS, ARENA, RADIO, DRAW, BADGES, SHOP, PUZZLE, CHALLENGE,
                               NEURO_HUB, NEURO_LESSON, Q_LESSON, TUNE_LESSON, TUNENET_LESSON, SELFPLAY_LESSON, METHOD_LESSON, EVO_LESSON, NEURO_TRAIN, ARENA_TRAIN,
                               LESSONS_MENU, CODE_LAB, CODE_LESSON, TRANSFER_LESSON, BRAIN_VIEW, BRAIN_MAP,
                               PILOT_LESSON, RNN_LESSON, PERCEPTION_LESSON, BACKPROP_LESSON, LIBRARY, TRAIN_PICK, LEVEL_SELECT };

  void gotoSelect();
  void gotoHome();
  void gotoIntro(uint32_t level);
  void gotoGame();

  // A long-press anywhere opens a small MENU modal (Home / Back / Sound / Close) -- the always-on
  // corner sound icon used to get in the way, so navigation + sound now live behind a hold gesture.
  bool _menuModal = false;
  uint32_t _pressStart = 0;         // millis the current touch began (0 = no touch)
  int16_t _pressX = 0, _pressY = 0; // where it began (to reject drags as long-presses)
  bool _longFired = false;          // the long-press already opened the menu this touch
  bool handleMenuUi(uint32_t now, const hal::TouchPoint& tp, bool tap);  // long-press detect + open modal; true if consumed
  void drawMenuModal();
  void goBack();                    // Menu "Back": re-enter the previous screen (else Home)
  void enterState(State s);         // re-enter a screen by state (used by goBack)
  State _prevState = State::HOME;   // the screen before the current one (for Back)
  State _stateAtTickStart = State::SELECT;

  bool _soundModal = false;
  bool _soundDown = false;          // last touch state, for rising-edge tap detection on the overlay
  void drawSoundModal();            // the volume / music / sfx overlay
  bool handleSoundUi(int tx, int ty, bool tapped);  // returns true if it consumed the tap
  void applyAndSaveSound();         // push audio state into settings + persist (if a profile is loaded)
  void returnToSub();        // BACK from Badges/Shop/Draw -> Home or Stats (whoever opened it)
  void drawIntro();
  void loadProfileInto(const std::string& id);
  void saveProfile();

  // Idle nametag screensaver (classroom fun): after a quiet minute on a resting screen, the
  // display becomes a big name card (avatar + name + stars); any touch wakes back to it.
  void drawNametag();          // paint the full-screen name card
  bool saverEligible() const;  // true only on calm menu states (never mid-game/training)
  void wake();                 // redraw the current screen after the saver is dismissed
  uint32_t _lastInput = 0;     // millis of the last touch
  bool _saver = false;         // the nametag is currently showing
  static constexpr uint32_t SAVER_MS = 60000;  // idle timeout (1 min)

  State _state = State::SELECT;
  gb::Profile _profile;
  uint32_t _introLevel = 1;
  const char* _newBadge = nullptr;  // achievement to celebrate on the next intro
  bool _subFromHome = false;        // did Badges/Shop/Draw open from the hub (vs Stats)?

  screens::ProfileSelectScreen _select;
  screens::ProfileCreateScreen _create;
  screens::HomeScreen _home;
  screens::LevelSelectScreen _levelSelect;
  screens::GameScreen _game;
  screens::StatsScreen _stats;
  screens::ArenaScreen _arena;
  screens::RadioScreen _radio;
  screens::PixelEditorScreen _pixed;
  screens::BadgesScreen _badges;
  screens::ShopScreen _shop;
  screens::PuzzleRaceScreen _puzzle;
  screens::ChallengeScreen _challenge;
  bool _inChallenge = false;  // the GAME screen is running a seed challenge, not campaign
  screens::NeuroLessonScreen _neuro;
  screens::LessonHubScreen _lessonHub;
  screens::QLessonScreen _qLesson;
  screens::TuneLessonScreen* _tuneLesson = nullptr;  // heap (its QLearn table is ~1.3KB; static DRAM is tight)
  screens::TuneNetLessonScreen* _tuneNetLesson = nullptr;  // heap (holds a Net ~1KB; static DRAM is tight)
  screens::SelfPlayLessonScreen* _selfPlayLesson = nullptr;  // heap (holds a Net)
  screens::MethodLessonScreen _methodLesson;
  screens::EvoLessonScreen _evoLesson;
  screens::NeuroTrainScreen _neuroTrain;
  screens::ArenaTrainScreen _arenaTrain;
  screens::LessonsMenuScreen _lessonsMenu;
  screens::CodeLabScreen _codeLab;
  screens::CodeLessonScreen _codeLesson;
  screens::TransferLessonScreen _transferLesson;
  screens::PilotLessonScreen _pilotLesson;
  screens::RnnLessonScreen _rnnLesson;
  screens::PerceptionLessonScreen _perceptionLesson;
  screens::BackpropLessonScreen _backpropLesson;
  screens::LibraryScreen _library;
  int _renameLibIdx = -1;   // library entry being renamed via the keyboard (-1 = none)
  int _editLibIdx = -1;     // library entry being edited in the GAME editor (-1 = none)
  // "train brain >" from the editor pops a chooser: maze trainer vs arena (fight) trainer.
  void drawTrainPick();     // the modal: pick which trainer to open for this brain
  int _pendingTrainIdx = -1;   // brain index the editor asked to train (-> chosen trainer)
  bool _arenaFromEditor = false;  // ARENA_TRAIN was opened from the editor (-> write back + return to editor)
  screens::BrainViewScreen _brainView;
  screens::BrainMapScreen _brainMap;
  TapDetector _introTap;
  ui::Rect _backBtn{90, 196, 140, 28};   // "< Back" on the level intro -> hub
  ui::Rect _learnBtn{0, 0, 0, 0};        // "Learn it >" on the intro when a power just unlocked
  int _introLesson = -1;                 // lesson to open from the intro (CodeLesson idx, or 100 = NeuroLab)
  bool _fromIntro = false;               // a lesson was opened from the intro -> return there on Back
};

}  // namespace app
