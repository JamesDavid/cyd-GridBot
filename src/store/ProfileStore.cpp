#include "store/ProfileStore.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include "game/ProgramJson.h"
#include "core/Util.h"

namespace store {

ProfileStore profiles;

static const char* DIR = "/profiles";
static const char* INDEX = "/profiles/index.json";

static std::string pathFor(const std::string& id) {
  return std::string(DIR) + "/" + id + ".json";
}

// ---- (de)serialization ----------------------------------------------------
static std::string genUuid() {
  // 128-bit random id as hex (esp_random is hardware-RNG on ESP32).
  char buf[33];
  for (int i = 0; i < 4; i++) {
    uint32_t r = esp_random();
    snprintf(buf + i * 8, 9, "%08x", (unsigned)r);
  }
  return std::string(buf);
}

static std::string toHex(const std::vector<uint8_t>& v) {
  static const char* H = "0123456789abcdef";
  std::string s;
  s.reserve(v.size() * 2);
  for (uint8_t b : v) { s += H[b >> 4]; s += H[b & 0xF]; }
  return s;
}
static int hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return 0;
}
static void fromHex(const char* s, std::vector<uint8_t>& v) {
  v.clear();
  if (!s) return;
  for (size_t i = 0; s[i] && s[i + 1]; i += 2)
    v.push_back((uint8_t)((hexNibble(s[i]) << 4) | hexNibble(s[i + 1])));
}

static void profileToJson(const gb::Profile& p, JsonObject o) {
  o["id"] = p.id;
  o["uuid"] = p.uuid;
  o["name"] = p.name;
  o["avatar"] = p.avatar;
  o["level"] = p.level;
  o["seedBase"] = p.seedBase;
  JsonObject u = o["unlocks"].to<JsonObject>();
  u["jump"] = p.unlocks.jump;
  u["repeat"] = p.unlocks.repeat;
  u["func"] = p.unlocks.func;
  u["sense"] = p.unlocks.sense;
  JsonObject s = o["settings"].to<JsonObject>();
  s["sound"] = p.settings.sound;
  s["animSpeed"] = p.settings.animSpeed;
  s["miniMap"] = p.settings.miniMap;
  s["music"] = p.settings.music;
  s["sfx"] = p.settings.sfx;
  s["volume"] = p.settings.volume;
  JsonObject st = o["stats"].to<JsonObject>();
  st["levelsCompleted"] = p.stats.levelsCompleted;
  st["totalRuns"] = p.stats.totalRuns;
  st["totalWins"] = p.stats.totalWins;
  st["bonks"] = p.stats.bonks;
  st["falls"] = p.stats.falls;
  st["starsTotal"] = p.stats.starsTotal;
  st["totalPlaySeconds"] = p.stats.totalPlaySeconds;
  st["currentStreak"] = p.stats.currentStreak;
  st["arenaWins"] = p.stats.arenaWins;
  st["threeStarWins"] = p.stats.threeStarWins;
  st["brainsTrained"] = p.stats.brainsTrained;
  st["neuroWins"] = p.stats.neuroWins;
  st["fightersSaved"] = p.stats.fightersSaved;
  st["gauntletBest"] = p.stats.gauntletBest;
  JsonArray ch = st["cmd"].to<JsonArray>();
  for (int i = 0; i < gb::CS_COUNT; i++) ch.add(p.stats.commandsUsed[i]);
  if (p.workLevel) {
    JsonObject w = o["work"].to<JsonObject>();
    w["level"] = p.workLevel;
    gb::programToJson(p.work, w["program"].to<JsonObject>());
  }
  if (!p.library.empty()) {
    JsonArray lib = o["library"].to<JsonArray>();
    for (const auto& e : p.library) {
      JsonObject le = lib.add<JsonObject>();
      le["name"] = e.name;
      le["src"] = (int)e.source;
      if (e.srcLevel) le["lvl"] = (int)e.srcLevel;
      gb::programToJson(e.program, le["program"].to<JsonObject>());
    }
  }
  if (!p.customChar.empty()) o["cchar"] = toHex(p.customChar);
  if (!p.customGoal.empty()) o["cgoal"] = toHex(p.customGoal);
  o["ach"] = p.achievements;
  o["coins"] = p.coins;
  o["shopColor"] = p.shopColor;
  o["shopEmoji"] = p.shopEmoji;
  o["ownedColors"] = p.ownedColors;
  o["ownedEmojis"] = p.ownedEmojis;
  if (!p.levelRecs.empty()) {   // per-level best: 3 bytes each (stars, blocks, par), hex-packed
    std::vector<uint8_t> b; b.reserve(p.levelRecs.size() * 3);
    for (const auto& r : p.levelRecs) { b.push_back(r.stars); b.push_back(r.bestBlocks); b.push_back(r.par); }
    o["levels"] = toHex(b);
  }
}

static void profileFromJson(JsonObjectConst o, gb::Profile& p) {
  p = gb::Profile{};
  p.id = (const char*)(o["id"] | "");
  p.uuid = (const char*)(o["uuid"] | "");
  p.name = (const char*)(o["name"] | "Player");
  p.avatar = o["avatar"] | 0;
  p.level = o["level"] | 1;
  p.seedBase = o["seedBase"] | 0;
  JsonObjectConst u = o["unlocks"];
  p.unlocks.jump = u["jump"] | false;
  p.unlocks.repeat = u["repeat"] | false;
  p.unlocks.func = u["func"] | false;
  p.unlocks.sense = u["sense"] | false;
  JsonObjectConst s = o["settings"];
  p.settings.sound = s["sound"] | true;
  p.settings.animSpeed = s["animSpeed"] | 1;
  p.settings.miniMap = s["miniMap"] | false;
  p.settings.music = s["music"] | true;
  p.settings.sfx = s["sfx"] | true;
  p.settings.volume = s["volume"] | 2;
  JsonObjectConst st = o["stats"];
  p.stats.levelsCompleted = st["levelsCompleted"] | 0;
  p.stats.totalRuns = st["totalRuns"] | 0;
  p.stats.totalWins = st["totalWins"] | 0;
  p.stats.bonks = st["bonks"] | 0;
  p.stats.falls = st["falls"] | 0;
  p.stats.starsTotal = st["starsTotal"] | 0;
  p.stats.totalPlaySeconds = st["totalPlaySeconds"] | 0;
  p.stats.currentStreak = st["currentStreak"] | 0;
  p.stats.arenaWins = st["arenaWins"] | 0;
  p.stats.threeStarWins = st["threeStarWins"] | 0;
  p.stats.brainsTrained = st["brainsTrained"] | 0;
  p.stats.gauntletBest = (uint8_t)(st["gauntletBest"] | 0);
  p.stats.neuroWins = st["neuroWins"] | 0;
  p.stats.fightersSaved = st["fightersSaved"] | 0;
  JsonArrayConst ch = st["cmd"];
  int i = 0;
  for (JsonVariantConst v : ch) { if (i < gb::CS_COUNT) p.stats.commandsUsed[i++] = v | 0; }
  if (o["work"].is<JsonObjectConst>()) {
    JsonObjectConst w = o["work"];
    p.workLevel = w["level"] | 0;
    if (w["program"].is<JsonObjectConst>())
      gb::programFromJson(w["program"].as<JsonObjectConst>(), p.work);
  }
  if (o["library"].is<JsonArrayConst>()) {
    for (JsonObjectConst le : o["library"].as<JsonArrayConst>()) {
      gb::LibEntry e;
      e.name = (const char*)(le["name"] | "");
      e.source = (uint8_t)(int)(le["src"] | 0);
      e.srcLevel = (uint16_t)(int)(le["lvl"] | 0);
      if (le["program"].is<JsonObjectConst>())
        gb::programFromJson(le["program"].as<JsonObjectConst>(), e.program);
      p.library.push_back(e);
    }
  }
  if (o["cchar"].is<const char*>()) fromHex(o["cchar"], p.customChar);
  if (o["cgoal"].is<const char*>()) fromHex(o["cgoal"], p.customGoal);
  p.achievements = o["ach"] | 0;
  p.coins = o["coins"] | 0;
  p.shopColor = o["shopColor"] | 0;
  p.shopEmoji = o["shopEmoji"] | 0;
  p.ownedColors = o["ownedColors"] | 0;
  p.ownedEmojis = o["ownedEmojis"] | 0;
  p.levelRecs.clear();
  if (o["levels"].is<const char*>()) {
    std::vector<uint8_t> b; fromHex(o["levels"], b);
    for (size_t i = 0; i + 3 <= b.size(); i += 3) {
      gb::LevelRec r; r.stars = b[i]; r.bestBlocks = b[i + 1]; r.par = b[i + 2];
      p.levelRecs.push_back(r);
    }
  }
}

// ---- store ops ------------------------------------------------------------
void ProfileStore::begin() {
  if (!LittleFS.exists(DIR)) LittleFS.mkdir(DIR);
  rebuildIndex();  // refresh the index (incl. cosmetics) so player-select cards are current
}

bool ProfileStore::load(const std::string& id, gb::Profile& out) {
  File f = LittleFS.open(pathFor(id).c_str(), "r");
  if (!f) return false;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;
  profileFromJson(doc.as<JsonObjectConst>(), out);
  return true;
}

bool ProfileStore::save(const gb::Profile& p) {
  File f = LittleFS.open(pathFor(p.id).c_str(), "w");
  if (!f) return false;
  JsonDocument doc;
  profileToJson(p, doc.to<JsonObject>());
  serializeJson(doc, f);
  f.close();
  rebuildIndex();
  return true;
}

// Per-level best programs live in their OWN directory, away from /profiles, so rebuildIndex() (which
// scans /profiles and treats every file there as a profile) never mistakes them for one.
static const char* LVLDIR = "/levels";
static std::string pathForLevel(const std::string& id, uint32_t level) {
  char b[48];
  snprintf(b, sizeof(b), "%s/%s_%u.json", LVLDIR, id.c_str(), (unsigned)level);
  return std::string(b);
}

bool ProfileStore::saveLevelProgram(const std::string& id, uint32_t level, const gb::Program& prog) {
  LittleFS.mkdir(LVLDIR);   // idempotent
  File f = LittleFS.open(pathForLevel(id, level).c_str(), "w");
  if (!f) return false;
  JsonDocument doc;
  gb::programToJson(prog, doc.to<JsonObject>());
  serializeJson(doc, f);
  f.close();
  return true;
}

bool ProfileStore::loadLevelProgram(const std::string& id, uint32_t level, gb::Program& out) {
  File f = LittleFS.open(pathForLevel(id, level).c_str(), "r");
  if (!f) return false;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;
  gb::programFromJson(doc.as<JsonObjectConst>(), out);
  return true;
}

bool ProfileStore::listProfiles(std::vector<ProfileMeta>& out) {
  out.clear();
  File f = LittleFS.open(INDEX, "r");
  if (!f) return true;  // no profiles yet
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;
  for (JsonObjectConst o : doc.as<JsonArrayConst>()) {
    ProfileMeta m;
    m.id = (const char*)(o["id"] | "");
    m.name = (const char*)(o["name"] | "");
    m.avatar = o["avatar"] | 0;
    m.level = o["level"] | 1;
    m.shopColor = o["shopColor"] | 0;
    m.shopEmoji = o["shopEmoji"] | 0;
    if (o["cchar"].is<const char*>()) fromHex(o["cchar"], m.customChar);
    out.push_back(m);
  }
  return true;
}

// Rebuild the index by scanning the profile files. Robust if a write was interrupted.
void ProfileStore::rebuildIndex() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  File dir = LittleFS.open(DIR);
  if (dir && dir.isDirectory()) {
    File entry;
    while ((entry = dir.openNextFile())) {
      std::string nm = entry.name();
      entry.close();
      // skip index.json itself; only take uXX.json
      if (nm.find("index") != std::string::npos) continue;
      if (nm.size() < 6) continue;
      std::string id = nm.substr(0, nm.find(".json"));
      gb::Profile p;
      if (load(id, p)) {
        JsonObject o = arr.add<JsonObject>();
        o["id"] = p.id;
        o["name"] = p.name;
        o["avatar"] = p.avatar;
        o["level"] = p.level;
        o["shopColor"] = p.shopColor;
        o["shopEmoji"] = p.shopEmoji;
        if (!p.customChar.empty()) o["cchar"] = toHex(p.customChar);  // so cards show the real avatar
      }
    }
  }
  File f = LittleFS.open(INDEX, "w");
  if (f) { serializeJson(doc, f); f.close(); }
}

std::string ProfileStore::createProfile(const std::string& name, uint8_t avatar) {
  // next free id uNN
  std::vector<ProfileMeta> metas;
  listProfiles(metas);
  int maxN = 0;
  for (auto& m : metas) {
    if (m.id.size() >= 2 && m.id[0] == 'u') {
      int n = atoi(m.id.c_str() + 1);
      if (n > maxN) maxN = n;
    }
  }
  char idbuf[8];
  snprintf(idbuf, sizeof(idbuf), "u%02d", maxN + 1);

  gb::Profile p;
  p.id = idbuf;
  p.uuid = genUuid();
  p.name = name.empty() ? "Player" : name;
  p.avatar = avatar;
  p.level = 1;
  // per-profile RNG salt so two kids see different mazes (SPEC §5.1)
  uint32_t h = 0x9E3779B9u + (uint32_t)(maxN + 1) * 2654435761u;
  for (char ch : p.name) h = gb::mix32(h ^ (uint8_t)ch);
  p.seedBase = h ? h : 1;
  p.unlocks = gb::computeUnlocks(1);
  save(p);
  return p.id;
}

bool ProfileStore::remove(const std::string& id) {
  bool ok = LittleFS.remove(pathFor(id).c_str());
  rebuildIndex();
  return ok;
}

}  // namespace store
