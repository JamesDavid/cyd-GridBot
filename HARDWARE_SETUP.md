# GridBot — hardware & build guide

Everything you need to build a GridBot. The [README](README.md) is the showcase; this is
the "boring detail" so that one stays a showcase.

## Supported target

| Board | PlatformIO env | Status |
|---|---|---|
| **CYD — Cheap Yellow Display, ESP32-2432S028R** (2.8" 320×240, ILI9341, XPT2046 resistive touch, no PSRAM) | `cyd_gridbot` | ✅ verified on hardware |
| Same board, on-device self-test harness | `cyd_gridbot_selftest` | ✅ prints PASS/FAIL over serial |
| Host (headless engine unit tests, no board) | `native` | ✅ ~40 Unity tests |

The 2-USB CYD variant sometimes ships with an **ST7789** panel instead of ILI9341 and may
need colour inversion / BGR order — GridBot ships configured for the known-good ILI9341,
with build-flag toggles in `platformio.ini` (`CYD_PANEL_RGB_ORDER`, `DISPLAY_DEFAULT_ROTATION`,
`CYD_PANEL_INVERT`) so you can fix it without touching code. See
[`CYD-ESP32-2432S028R.md`](CYD-ESP32-2432S028R.md).

## Bill of materials

- **1 × ESP32-2432S028R "Cheap Yellow Display"** (~$10–15; search "CYD ESP32 2.8 inch").
  One that works: [CYD on Amazon](https://a.co/d/05H98aWQ).
- **1 × USB cable** (micro-USB or USB-C depending on your board revision) for flashing.
- That's it. Optional: a second CYD for radio battle/trade.

No SD card, no RTC, no network hardware — GridBot is fully offline.

## Toolchain & build

1. Install [PlatformIO](https://platformio.org/) (the CLI, or the VS Code extension).
   On Windows it is not on `PATH`; the venv binary is at
   `C:/Users/<you>/.platformio/penv/Scripts/platformio.exe`.
2. Clone this repo and build the firmware (compile only, no upload):
   ```bash
   platformio run -e cyd_gridbot
   ```
   A green build prints a `RAM: … Flash: …` table and `[SUCCESS]`.
3. Run the host engine tests (needs a host C/C++ compiler, e.g. MSYS2/MinGW on Windows):
   ```bash
   platformio test -e native
   ```

## First flash (USB)

```bash
platformio run -e cyd_gridbot -t upload --upload-port COM3   # your serial port
```

On first boot GridBot mounts/format's LittleFS and, on a resistive panel, runs a one-time
**4-corner touch calibration** (tap the targets). Calibration is stored and tagged with the
display rotation, so it auto-re-runs only if you change the orientation.

To confirm the engine on real hardware, flash the self-test build and watch the serial
monitor at 115200 — it prints `PASS`/`FAIL` per fixture and a final `SELFTEST RESULT`:
```bash
platformio run -e cyd_gridbot_selftest -t upload --upload-port COM3
platformio device monitor -p COM3 -b 115200
```

## The headless dev loop (optional)

GridBot has a USB-serial automation loop (no WiFi needed) — handy for development and for
capturing screenshots:

- `python scripts/shot.py COM3 out.png` — sends `S`, reads the panel back, writes a PNG.
- `python scripts/drive.py COM3 "tap 80 50; wait .3; shot a.png"` — inject taps + screenshot.
- Firmware serial commands: `S` screenshot · `T x y` tap · `L` toggle touch logging ·
  `X` clear calibration + reboot · `H` go to the home screen · `G n` jump to level n ·
  `P n` auto-play to level n.

Run these with PlatformIO's bundled Python (it has `pyserial`):
`C:/Users/<you>/.platformio/penv/Scripts/python.exe scripts/shot.py COM3 .pio/shot.png`.

## Troubleshooting

- **Blank/white screen** → wrong panel driver (you may have an ST7789 unit); see the panel
  toggles above and `CYD-ESP32-2432S028R.md`.
- **Colours look wrong** (cyan instead of yellow, etc.) → flip `CYD_PANEL_RGB_ORDER`.
- **Taps land in the wrong place** → the panel needs re-calibration; send the `X` serial
  command (or change rotation, which auto-invalidates the stored calibration).
- **Upload fails / port busy** → close any serial monitor first; some CH340 boards need a
  manual BOOT-button press to enter the bootloader.
- **`platformio` not found** → use the venv path; it isn't on `PATH`.

## Board quirks reference

The display orientation, brick-wall/battery colour handling, the panel readback colour
mapping used by the serial screenshot, the touch point-reflection fix, and the no-PSRAM RAM
budget are all documented in [`CYD-ESP32-2432S028R.md`](CYD-ESP32-2432S028R.md). Read it
before changing anything display- or touch-related.
