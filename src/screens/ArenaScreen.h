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
  void enterRoom();   // jump straight into the multi-device tournament Room lobby (from Radio screen)
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;
  void debugStep();   // capture aid: advance the match one tick, paused
  // Jump straight into a battle (skip the menus): field library fighter `libIdx` vs the named
  // opponent. Used by the trainer's "Fight! >" so training flows directly into a match.
  void beginQuickBattle(gb::Profile* profile, int libIdx, const char* oppName, gb::MatchType type);

 private:
  enum class Phase : uint8_t { MENU, GAMETYPE, PICK1, HANDOFF, PICK2, BOARD, DONE, TOURNEY,
                               TDISC,     // tournament: pick discipline (Maze race vs Battle)
                               TPICK,     // tournament: pick which fighters take part (checklist + all)
                               TCHOICE,   // pick tournament format: Ladder (round-robin) or Cup (bracket)
                               CUP,       // bracket card shown between/after Cup matches
                               NETLOBBY };// multi-device room: host/join + roster, then a networked Cup
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
  void setupMatchBot(int pick, const gb::Pose& start);
  void genMatchBoard(uint32_t seed);   // build the maze/ring/pitch for the current _type
  void startMatch();
  void drawBoard();
  void mazeGeometry(int& tile, int& ox, int& oy);
  void drawCell(int r, int c);
  void eraseBotAt(int r, int c);   // clear a vacated cell + its neighbours (the facing arrow over-
                                   // hangs into the next tile, so redrawing one cell leaves artifacts)
  void drawBot(int i, const gb::Pose& p, int avatar);
  void drawBall();                 // Soccer: the ball at its current tile (white disc)
  void drawScore();                                  // big HP/score strip in the top bar (Battle)
  void drawHit(int i, const char* txt, uint16_t col);// burst over bot i when it's zapped/knocked
  void finishOverlay();
  void onMatchEnd();                                 // a BOARD match resolved: Cup result vs normal DONE
  // Tournament setup: pick discipline (maze race vs battle), then which fighters take part.
  void drawTDisc();       // discipline chooser (Maze race / Battle / Soccer)
  void drawTPick();       // participant checklist (per-bot checkbox + Select all)
  // The tournament discipline is just `_type` (Race / Sumo / Soccer); set in the chooser.
  uint32_t _tSelMask = 0; // selection bitmask, bit i = candidate i is in the tournament
  int _tScroll = 0;       // first visible row in the participant checklist
  // Tournament: round-robin every fighter vs every other, tally wins, rank them (a battle ladder).
  void runTournament(const std::vector<int>& parts);   // play all matches headless + sort standings
  void drawTourney();     // the leaderboard (paginated, 5 per page)
  void drawBreakdown();   // one fighter's per-opponent win/loss record (tap a standing)
  struct Standing { int cand; int w; int l; int mi; };  // mi = row index in the results matrix
  std::vector<Standing> _standings;
  std::vector<int> _tOrder;     // cand index for each results-matrix slot (matrix order)
  std::vector<int8_t> _tWin;    // N*N: +1 if row beat col, -1 if lost, 0 none (the full results)
  int16_t _tN = 0;              // matrix dimension (participant count)
  int16_t _standPage = 0;       // standings page (5 per page)
  int16_t _breakdownMi = -1;    // matrix row whose breakdown is shown (-1 = the standings list)
  int16_t _bdPage = 0;          // breakdown page
  // Single-elimination Cup: watch your fighters battle through a bracket to one champion. The
  // local version (library fighters) is the verifiable core the networked tournament will drive.
  void startCup(const std::vector<int>& parts);  // seed the bracket from the chosen fighters
  void cupAdvanceToNextMatch();  // skip byes, set up the next real match (or finish the Cup)
  void drawCupCard();     // the between-match bracket card (round, matchup, or champion)
  void drawTChoice();     // Ladder vs Cup chooser
  gb::Bracket _cupBracket;
  std::vector<int> _cupPlayers;  // candidate indices entered in the Cup (parallel to bracket ids)
  int _cupMatch = -1;            // bracket match index currently being played (-1 = none)
  bool _cup = false;             // a BOARD match is part of the Cup (route its result to the bracket)
  // History of every Cup game so the bracket card can draw ALL rounds as a tree (the live Bracket
  // only keeps the current round). a/b are player ids (b=-1 bye); win is the winning player id.
  struct CupGame { int8_t round; int8_t a; int8_t b; int8_t win; int8_t ga; int8_t gb; };  // ga/gb = soccer score
  std::vector<CupGame> _cupLog;
  void drawBracket();            // the full bracket tree (rounds as columns)
  // Networked room (multi-device ESP-NOW Cup): the field comes from the lobby roster and every
  // device replays the SAME matches from the shared seed (deterministic Arena -> same result).
  void drawNetLobby();           // host/join + the live roster
  void buildRosterField();       // turn the lobby roster into _cands + _cupPlayers, start the Cup
  bool _netCup = false;          // this Cup's match boards use the shared seed (not millis())
  uint32_t _netSeed = 0;         // the host's broadcast seed
  uint32_t _cupAutoAt = 0;       // networked Cup auto-advances at this time (every device in step)
  int8_t _roomN = -1;            // last-drawn roster size (redraw the lobby when it changes)

  gb::Profile* _profile = nullptr;
  std::vector<Candidate> _cands;
  gb::MatchType _type = gb::MatchType::RACE;
  bool _hotseat = false;
  int _pick0 = -1, _pick1 = -1;
  int _pickScroll = 0;  // first visible candidate row in the (scrollable) pick list
  uint32_t _sumoNonce = 0;  // bumped each Sumo match so the ring varies (local play)

  gb::Maze _maze;
  gb::Pose _s0, _s1;
  gb::Pose _ball, _goal0, _goal1;   // Soccer: ball + the two goal mouths (bot 0 attacks _goal0)
  gb::Pose _ballPrev;               // last-drawn ball cell (erase it before redrawing the ball)
  gb::Arena _arena;
  Phase _phase = Phase::MENU;
  bool _running = false;
  uint32_t _last = 0;
  app::TapDetector _tap;
};

}  // namespace screens
