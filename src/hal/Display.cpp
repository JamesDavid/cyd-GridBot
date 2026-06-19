#include "hal/Display.h"
#include <Arduino.h>

namespace hal {

Display display;

// Backlight on GPIO21 via LEDC channel 7 at a LOW frequency (<=5 kHz): the
// on-board MOSFET can't switch fast above ~10 kHz, and LovyanGFX's Light_PWM
// did not drive this unit (CYD-ESP32-2432S028R.md "backlight").
static constexpr int  BL_PIN = 21;
static constexpr int  BL_CH  = 7;
static constexpr int  BL_FREQ = 5000;
static constexpr int  BL_RES  = 8;

void Display::begin() {
  _lcd.init();
  _lcd.setRotation(DISPLAY_DEFAULT_ROTATION);

  ledcSetup(BL_CH, BL_FREQ, BL_RES);
  ledcAttachPin(BL_PIN, BL_CH);
  ledcWrite(BL_CH, 255);

  _lcd.fillScreen(TFT_BLACK);
}

void Display::setBacklight(uint8_t level) {
  ledcWrite(BL_CH, level);
}

}  // namespace hal
