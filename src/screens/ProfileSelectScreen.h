// GridBot — PROFILE_SELECT: a grid of kid cards + "New Player" (SPEC §4). Tap a
// card to play; tap its lower "stats" strip for the stats screen; long-press to
// delete (SPEC §16.6 default).
#pragma once
#include <vector>
#include "app/Screen.h"
#include "ui/UI.h"
#include "store/ProfileStore.h"

namespace screens {

class ProfileSelectScreen : public app::IScreen {
 public:
  void begin(store::ProfileStore* store);
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

  const std::string& chosenId() const { return _chosenId; }

 private:
  void draw();
  ui::Rect cardRect(int i) const;

  store::ProfileStore* _store = nullptr;
  std::vector<store::ProfileMeta> _metas;
  std::string _chosenId;

  app::TapDetector _tap;
  bool _pressActive = false, _longFired = false;
  int _pressIdx = -1;
};

}  // namespace screens
