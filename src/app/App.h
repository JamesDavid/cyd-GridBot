// GridBot — top-level app / state machine (SPEC §3, §11).
// PROFILE_SELECT -> PROFILE_CREATE / LEVEL_INTRO -> GAME -> (WIN) -> LEVEL_INTRO ; STATS.
#pragma once
#include <stdint.h>
#include "app/Screen.h"
#include "game/Profile.h"
#include "screens/GameScreen.h"
#include "screens/ProfileSelectScreen.h"
#include "screens/ProfileCreateScreen.h"
#include "screens/StatsScreen.h"
#include "screens/ArenaScreen.h"
#include "screens/RadioScreen.h"
#include "screens/PixelEditorScreen.h"
#include "screens/BadgesScreen.h"
#include "screens/ShopScreen.h"

namespace app {

class App {
 public:
  void begin();
  void tick(uint32_t now);
  void debugGoToLevel(uint32_t level);    // playtest aid (serial 'G <n>')
  void debugFastPlay(uint32_t target);    // auto-solve+win levels up to target ('P <n>')
  void debugHome();                       // force the profile-select screen ('H')
  void debugGrantCoins(uint32_t n);       // grant coins for testing ('C <n>')
  void debugAutoRun();                    // GAME: solve + run paused ('A')
  void debugStep();                       // advance the current run/match one tick ('N')

 private:
  enum class State : uint8_t { SELECT, CREATE, INTRO, GAME, STATS, ARENA, RADIO, DRAW, BADGES, SHOP };

  void gotoSelect();
  void gotoIntro(uint32_t level);
  void gotoGame();
  void drawIntro();
  void loadProfileInto(const std::string& id);
  void saveProfile();

  State _state = State::SELECT;
  gb::Profile _profile;
  uint32_t _introLevel = 1;
  const char* _newBadge = nullptr;  // achievement to celebrate on the next intro

  screens::ProfileSelectScreen _select;
  screens::ProfileCreateScreen _create;
  screens::GameScreen _game;
  screens::StatsScreen _stats;
  screens::ArenaScreen _arena;
  screens::RadioScreen _radio;
  screens::PixelEditorScreen _pixed;
  screens::BadgesScreen _badges;
  screens::ShopScreen _shop;
  TapDetector _introTap;
  ui::Rect _arenaBtn{90, 196, 140, 28};  // shown on the level intro post-sensing
};

}  // namespace app
