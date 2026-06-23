// GridBot — PROFILE_CREATE: large-key on-screen keyboard (max 8 chars) + character
// pick (SPEC §4, §16.1 default). Big targets for resistive touch.
#pragma once
#include <string>
#include "app/Screen.h"
#include "ui/UI.h"

namespace screens {

class ProfileCreateScreen : public app::IScreen {
 public:
  void begin();                                   // new profile
  void beginEdit(const std::string& name, uint8_t avatar);  // edit existing
  void beginRename(const std::string& name);      // rename a library bot (name only, no avatar)
  void enter() override;
  app::Signal tick(uint32_t now, const hal::TouchPoint& tp) override;

  const std::string& name() const { return _name; }
  uint8_t avatar() const { return _avatar; }
  bool isEdit() const { return _edit; }

 private:
  void draw();
  void drawNameField();
  ui::Rect keyRect(int i) const;
  ui::Rect avatarRect(int i) const;

  std::string _name;
  uint8_t _avatar = 0;
  bool _edit = false;
  bool _rename = false;   // renaming a library bot: title "Rename bot", no avatar row
  app::TapDetector _tap;
};

}  // namespace screens
