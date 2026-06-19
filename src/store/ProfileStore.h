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
};

class ProfileStore {
 public:
  void begin();
  bool listProfiles(std::vector<ProfileMeta>& out);
  bool load(const std::string& id, gb::Profile& out);
  bool save(const gb::Profile& p);                 // writes file + refreshes index
  std::string createProfile(const std::string& name, uint8_t avatar);
  bool remove(const std::string& id);

 private:
  void rebuildIndex();
};

extern ProfileStore profiles;

}  // namespace store
