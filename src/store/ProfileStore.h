// GridBot — profile persistence on LittleFS (SPEC §5.1, §14). Board-only (uses
// LittleFS), so it lives outside src/game and never enters the native build.
#pragma once
#include <string>
#include <vector>
#include "game/Profile.h"

namespace store {

struct ProfileMeta {
  std::string id;
  std::string name;
  uint8_t avatar = 0;
  uint32_t level = 1;
  // cosmetics, so the player-select cards can show the real avatar (custom drawing / tint / emoji)
  uint8_t shopColor = 0;
  uint8_t shopEmoji = 0;
  std::vector<uint8_t> customChar;  // 16x16 KidPix sprite (empty = none)
};

class ProfileStore {
 public:
  void begin();
  bool listProfiles(std::vector<ProfileMeta>& out);
  bool load(const std::string& id, gb::Profile& out);
  bool save(const gb::Profile& p);                 // writes file + refreshes index
  std::string createProfile(const std::string& name, uint8_t avatar);
  bool remove(const std::string& id);

  // Per-level best PROGRAM, kept in its own flash file so it's not held in RAM with the profile.
  // Written when a level is beaten with a new fewest-blocks best; loaded when reopening to revise.
  bool saveLevelProgram(const std::string& id, uint32_t level, const gb::Program& prog);
  bool loadLevelProgram(const std::string& id, uint32_t level, gb::Program& out);

  // Device-wide sound settings (the speaker is shared, not per-kid). Persisted in their own file so
  // they survive reboots and apply on the menu BEFORE any profile is picked.
  void saveAudio(bool sound, bool music, bool sfx, uint8_t volume);
  bool loadAudio(bool& sound, bool& music, bool& sfx, uint8_t& volume);  // false if never saved

 private:
  void rebuildIndex();
};

extern ProfileStore profiles;

}  // namespace store
