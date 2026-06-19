// GridBot — LovyanGFX panel + touch config for the CYD ESP32-2432S028R.
// Sourced verbatim-in-spirit from CYD-ESP32-2432S028R.md ("known-good"): do NOT
// re-derive pins or the panel driver. The panel is driven landscape-native MV=0
// (panel_width=320), with setRotation(6) applied in Display::begin().
//
// If the real unit shows the §1.1 symptoms (blank/white -> wrong driver;
// white<->black -> flip `invert`; red/blue swapped -> flip CYD_PANEL_RGB_ORDER;
// mirrored/offset -> change DISPLAY_DEFAULT_ROTATION), change ONLY the flags
// below / in platformio.ini — no other code touches panel params.
#pragma once

#include <LovyanGFX.hpp>

#ifndef DISPLAY_DEFAULT_ROTATION
#define DISPLAY_DEFAULT_ROTATION 6
#endif
#ifndef CYD_PANEL_RGB_ORDER
#define CYD_PANEL_RGB_ORDER 1
#endif
#ifndef CYD_PANEL_INVERT
#define CYD_PANEL_INVERT 0
#endif

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 _panel;
  lgfx::Bus_SPI       _bus;
  lgfx::Touch_XPT2046 _touch;

 public:
  LGFX() {
    {  // Display bus: HSPI. MOSI 13 / SCLK 14 / MISO 12 / DC 2.
      auto c = _bus.config();
      c.spi_host    = HSPI_HOST;
      c.spi_mode    = 0;
      c.freq_write  = 40000000;
      c.freq_read   = 16000000;
      c.spi_3wire   = false;
      c.use_lock    = true;
      c.dma_channel = SPI_DMA_CH_AUTO;
      c.pin_sclk    = 14;
      c.pin_mosi    = 13;
      c.pin_miso    = 12;
      c.pin_dc      = 2;
      _bus.config(c);
      _panel.setBus(&_bus);
    }
    {  // Panel ILI9341, landscape-native (MV=0). CS 15 / RST -1 / BL handled by LEDC.
      auto c = _panel.config();
      c.pin_cs          = 15;
      c.pin_rst         = -1;
      c.pin_busy        = -1;
      c.panel_width     = 320;
      c.panel_height    = 240;
      c.memory_width    = 320;
      c.memory_height   = 240;
      c.offset_x        = 0;
      c.offset_y        = 0;
      c.offset_rotation = 0;
      c.dummy_read_pixel = 8;
      c.dummy_read_bits  = 1;
      c.readable        = true;
      c.invert          = (CYD_PANEL_INVERT != 0);
      c.rgb_order       = (CYD_PANEL_RGB_ORDER != 0);
      c.dlen_16bit      = false;
      c.bus_shared      = false;  // touch is on its OWN VSPI bus
      _panel.config(c);
    }
    {  // Touch XPT2046 on its own VSPI bus: CLK 25 / MOSI 32 / MISO 39 / CS 33 / IRQ 36.
      auto t = _touch.config();
      t.x_min      = 200;
      t.x_max      = 3900;
      t.y_min      = 200;
      t.y_max      = 3900;
      t.pin_int    = 36;
      t.bus_shared = false;
      t.offset_rotation = 0;
      t.spi_host   = VSPI_HOST;
      t.freq       = 1000000;
      t.pin_sclk   = 25;
      t.pin_mosi   = 32;
      t.pin_miso   = 39;
      t.pin_cs     = 33;
      _touch.config(t);
      _panel.setTouch(&_touch);
    }
    setPanel(&_panel);
  }
};
