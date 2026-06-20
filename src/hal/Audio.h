// GridBot — tone helper on the speaker / DAC pin (GPIO26) via LEDC tone (SPEC §13).
// Short SFX are blocking; a background melody is non-blocking (advanced from the main
// loop via update()). SFX briefly override the melody, which resumes on the next note.
// All calls are no-ops when sound is disabled.
#pragma once
#include <stdint.h>

namespace hal {

struct Note { uint16_t freq; uint16_t ms; };  // freq 0 = rest

class Audio {
 public:
  void begin();
  void setEnabled(bool on);
  bool enabled() const { return _on; }
  void blip();   // tap feedback
  void tick();   // step tick
  void win();    // happy fanfare
  void fail();   // sad buzz
  void badge();  // achievement chime

  // non-blocking background music
  void startMusic(const Note* notes, int count, bool loop);
  void stopMusic();
  void update(uint32_t now);
  bool playing() const { return _playing; }

 private:
  void tone(uint16_t freq, uint16_t ms);

  bool _on = true;
  const Note* _mel = nullptr;
  int _melLen = 0, _melIdx = 0;
  bool _melLoop = false, _playing = false;
  uint32_t _noteStart = 0;
};

extern Audio audio;

// Shared melodies (defined in Audio.cpp).
extern const Note kTitleMusic[];
extern const int kTitleMusicLen;
extern const Note kArenaMusic[];
extern const int kArenaMusicLen;

}  // namespace hal
