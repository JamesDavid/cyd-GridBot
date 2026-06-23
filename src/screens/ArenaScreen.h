// GridBot — Arena (SPEC §18). Pre-match menu (Race/Sumo + opponent), then an
// animated deterministic match. Opponents: House AI, Hotseat 2P (lock-in/handoff),
// and Radio (ESP-NOW, hardware-pending). Authoring-in-code-view -> BACKLOG; bots are
// picked from built-ins + the player's library.
#pragma once
#include <vector>
#include <string>
#include "app/Screen.h"
#include "ui/UI.h"
#include "game/Maze.h"
#include "game/Arena.h"
#include "game/Bracket.h"
#include "game/Profile.h"

namespace screens {

class ArenaScreen : public app::IScreen {
 public:
  void begin(gb::Profile* profile);
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;
  void debugStep();   // capture aid: advance the match one tick, paused
  // Jump straight into a battle (skip the menus): field library fighter `libIdx` vs the named
  // opponent. Used by the trainer's "Fight! >" so training flows directly into a match.
  void beginQuickBattle(gb::Profile* profile, int libIdx, const char* oppName, bool sumo);

 private:
  enum class Phase : uint8_t { MENU, GAMETYPE, PICK1, HANDOFF, PICK2, BOARD, DONE, TOURNEY,
                               TCHOICE,   // pick tournament format: Ladder (round-robin) or Cup (bracket)
                               CUP };     // bracket card shown between/after Cup matches
  struct Candidate { std::string name; gb::Program prog; uint8_t avatar; std::string style; bool house; bool smart; bool neuro = false; };

  void buildCandidates(bool sumo = false);  // sumo => only NeuroBot fighters, no dashers
  void drawMenu();
  void drawGameType();
  void drawPick(int player);
  ui::Rect pickRowRect(int i) const;
  int pickVisible() const;
  bool pickScrollTap(int x, int y);  // handle a tap in the scrollbar zone
  int houseBotIndex(const char* name) const;
  void drawHandoff();
  void setupMatchBot(int pick, const gb::Pose& start, bool sumo);
  void startMatch();
  void drawBoard();
  void mazeGeometry(int& tile, int& ox, int& oy);
  void drawCell(int r, int c);
  void eraseBotAt(int r, int c);   // clear a vacated cell + its neighbours (the facing arrow over-
                                   // hangs into the next tile, so redrawing one cell leaves artifacts)
  void drawBot(int i, const gb::Pose& p, int avatar);
  void drawScore();                                  // big HP/score strip in the top bar (Battle)
  void drawHit(int i, const char* txt, uint16_t col);// burst over bot i when it's zapped/knocked
  void finishOverlay();
  void onMatchEnd();                                 // a BOARD match resolved: Cup result vs normal DONE
  // Tournament: round-robin every fighter vs every other, tally wins, rank them (a battle ladder).
  void runTournament();   // play all the matches headless + sort the standings
  void drawTourney();     // the leaderboard
  struct Standing { int cand; int w; int l; };
  std::vector<Standing> _standings;
  // Single-elimination Cup: watch your fighters battle through a bracket to one champion. The
  // local version (library fighters) is the verifiable core the networked tournament will drive.
  void startCup();        // build the field from the library + seed the bracket
  void cupAdvanceToNextMatch();  // skip byes, set up the next real match (or finish the Cup)
  void drawCupCard();     // the between-match bracket card (round, matchup, or champion)
  void drawTChoice();     // Ladder vs Cup chooser
  gb::Bracket _cupBracket;
  std::vector<int> _cupPlayers;  // candidate indices entered in the Cup (parallel to bracket ids)
  int _cupMatch = -1;            // bracket match index currently being played (-1 = none)
  bool _cup = false;             // a BOARD match is part of the Cup (route its result to the bracket)

  gb::Profile* _profile = nullptr;
  std::vector<Candidate> _cands;
  gb::MatchType _type = gb::MatchType::RACE;
  bool _hotseat = false;
  int _pick0 = -1, _pick1 = -1;
  int _pickScroll = 0;  // first visible candidate row in the (scrollable) pick list
  uint32_t _sumoNonce = 0;  // bumped each Sumo match so the ring varies (local play)

  gb::Maze _maze;
  gb::Pose _s0, _s1;
  gb::Arena _arena;
  Phase _phase = Phase::MENU;
  bool _running = false;
  uint32_t _last = 0;
  app::TapDetector _tap;
};

}  // namespace screens
