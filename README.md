# SolderingStation

[WIO Terminal](https://wiki.seeedstudio.com/Wio_Terminal_Intro/) adaptation of the [Elektor Platino soldering station](https://www.elektormagazine.com/magazine/elektor-201507/27978)

## Development environment

### Tooling

- [Visual Studio Code](https://code.visualstudio.com/)
- [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode) for VS Code (recommended in `.vscode/extensions.json`)
  - Installs its own toolchain (GCC ARM, `pio` CLI, etc.) on first use — no separate Arduino IDE or ARM toolchain install needed
- [Teleplot](https://teleplot.fr/) VS Code extension (optional) — for live-plotting the PID tuning telemetry printed over serial, see [Telemetry / PID tuning](#telemetry--pid-tuning) below
- [KiCad 10](https://www.kicad.org/) — for the front panel PCB, see [Front panel](#front-panel) below

### Hardware target

- [Seeed WIO Terminal](https://wiki.seeedstudio.com/Wio_Terminal_Intro/) (SAMD51, Cortex-M4F)
- `atmelsam` platform, `board = seeed_wio_terminal`, `framework = arduino` (see `platformio.ini`)

### Libraries

Declared in `platformio.ini` under `lib_deps`, fetched automatically by PlatformIO on build:

- [LovyanGFX](https://github.com/lovyan03/LovyanGFX) — drives the 2.4" ILI9341 LCD (`#include <LovyanGFX.hpp>`, class `LGFX`), using genuine DMA-based SPI for speed. Needs `-D LGFX_AUTODETECT -D LGFX_WIO_TERMINAL` in `platformio.ini`'s `build_flags`, since PlatformIO's `seeed_wio_terminal` board definition doesn't define `ARDUINO_WIO_TERMINAL`, which LovyanGFX's autodetect normally keys off.
- [SparkFun_Qwiic_Twist_Arduino_Library](https://github.com/sparkfun/SparkFun_Qwiic_Twist_Arduino_Library) — driver for the Qwiic Twist rotary encoder, connected via a Grove-to-Qwiic cable on the WIO Terminal's Grove I2C port
- [ETL (Embedded Template Library)](https://github.com/ETLCPP/etl) — used throughout for containers/utilities in this interrupt-driven design
- [Arduino-PID-Library](https://github.com/br3ttb/Arduino-PID-Library) — drives the tip-temperature PID control loop

### Building and uploading

Build and upload using the PlatformIO sidebar in VS Code (project/build/upload icons in the PlatformIO toolbar), or the equivalent `pio run` / `pio run -t upload` commands from a PlatformIO Core CLI.

### Known build gotchas

- `Arduino.h` `#define`s `round(x)` as a macro, which breaks ETL's `etl::to_string`. Add `#undef round` right after `#include <Arduino.h>` and before any `etl/*` includes in any file that includes both.
- LovyanGFX's rotation numbering for this board is offset by 180° from the old TFT_eSPI-based fork this project used to use — call `tft.setRotation(1)` (not `3`) for correct upright landscape orientation.

## Hardware

### Inputs

- Three onboard buttons (`WIO_KEY_A`/`B`/`C`, pins 28/29/30). Physical left-to-right position does **not** match alphabetical pin naming — confirmed on-device: left = `WIO_KEY_C`, middle = `WIO_KEY_B`, right = `WIO_KEY_A`.
- [SparkFun Qwiic Twist](https://www.sparkfun.com/products/15083) rotary encoder, connected via Grove-to-Qwiic cable into the WIO Terminal's Grove I2C port (standard `Wire`, no special Qwiic-specific setup needed). Its INT pad is wired to `BCM21` (40-pin header physical position 40) — confirmed working. **Do not** wire it to `BCM2` (physical position 3): despite the Arduino pin table labeling it as a free, independent I2C bus, it shares a net at the PCB level with the Grove I2C bus and breaks the Twist's communication when wired, even before any new firmware is uploaded.
  - The Twist library never enables its own interrupt register (`TWIST_ENABLE_INTS`, `0x04`) — `setup()` writes it directly over raw I2C (`0x03`, encoder + button interrupt bits) to get the INT line to assert at all.
  - `twist.setIntTimeout(0)` is set to avoid the default ~250ms coalescing delay on rotation events, for a responsive display.

### Tip temperature sensing

Signal chain from the soldering tip's thermocouple to the ADC reading (see `TipTemperature.h`):

```
tip sensor (16uV/°C) -> PCB amplifier (gain 68000/150) -> ADC (12-bit, 3.3V ref, pin A3/BCM24/40-pin header pin 18)
```

### Heater and power supply

- The soldering iron's heating element measures ~2Ω and is rated 40W (implied design voltage ≈ 9–12V, not the 19V a laptop supply would suggest).
- Power supply: **12V, 5A** (not the originally-considered 19V/3A laptop brick, which was bench-tested and found both underpowered — couldn't supply the ~9.5A peak current the element draws at 19V without significant ripple — and would have driven the element to ~4.5x its rated power at full duty).
- The reverse-polarity protection diode on the supply input is a **Schottky** diode (not silicon) — its lower forward voltage drop (~0.2–0.3V vs. ~0.7V) leaves more of the supply's voltage headroom for the heater.
- PWM output pin: `BCM23`/A2.

⚠️ **Safety note:** at the time of writing, this firmware has no firmware-enforced maximum duty cap, no sensor fault-checking (e.g. a disconnected sensor reading as "too cold" and driving the PID to sustained high duty), and no hardware watchdog. Until these are implemented, do not leave the station heating unattended.

### Front panel

A PCB used as the enclosure's front panel (black soldermask, white silkscreen), KiCad 10 project in `hardware/frontpanel/`. It carries the mains switch, the WIO Terminal (plugged into a 40-pin header on the front), the Qwiic Twist knob (on the back, plugged into a socket) and the XLR socket for the iron. All routing is on the back layer, so nothing shows through the soldermask on the front.

The board is generated by `generate_frontpanel.py`. To adapt it to your own enclosure and parts, edit the constants at the top of the script and run it with the Python interpreter that comes with KiCad 10:

```
<KiCad install dir>/bin/python generate_frontpanel.py
```

The main parameters (all in mm, from the top-left corner as seen from the front):

- `PANEL_W`, `PANEL_H`, `PANEL_THICKNESS`, `SLOT_DEPTH` — panel size and how far it sits in the enclosure's slots.
- `SWITCH_X`, `WIO_X`, `KNOB_X`, `XLR_X` — horizontal position of each part; everything is centred vertically. The connectors, tracks and silkscreen follow.
- `SWITCH_HOLE_D`, `ENCODER_HOLE_D`, `XLR_HOLE_D`, `XLR_SCREW_DX` — cutouts for your particular parts.
- `BACK_CONN_POS`, `BACK_CONN_EXIT` — where the connector to the iron PCB sits and which way its cable leaves.
- `CREDITS`, `REPO_URL` — text and QR code on the back.

After regenerating, open the board in KiCad and run DRC. The script overwrites `frontpanel.kicad_pcb`, so either keep making changes in the script, or edit in KiCad and stop using the script.

The connector to the soldering iron PCB (J3, 4-pin JST-PH) — the iron PCB must use the same order:

| Pin | Signal | WIO |
|---|---|---|
| 1 | TEMP (tip temperature, ADC) | `A3`/`BCM24` |
| 2 | GND | |
| 3 | HEATER PWM | `BCM23` |
| 4 | 5V (powers the WIO) | |

A second 4-pin JST-PH (J4) brings out three spare GPIOs (`BCM7`, `BCM12`, `BCM16`) and GND for debugging.

Things to watch out for:

- The WIO's 40-pin header follows the Raspberry Pi pinout exactly. The pin table in the board's `variant.h` wrongly shows pin 17 as GND; it's 3.3V.
- The Twist's encoder is on its bottom side, so seen from the front its pin row is mirrored relative to SparkFun's drawings.
- The copper pours stay clear of the board edge and of the XLR and mains switch, so logic GND can't touch the earthed enclosure, the XLR chassis or mains.
- The panel must fill the enclosure's slots (2.2 mm here). A 2.0 mm board costs about four times as much as a 1.0 or 1.6 mm one, so here two identical 1.0 mm boards are stacked (`PANEL_THICKNESS` = 1.0): the back one carries the parts, the front one only shows its front side, and the parts' screws clamp them together. The 40-pin header then goes through both boards — use one with pins long enough to solder on the back of the back board.
- Have the fab leave its order number off the board (at JLCPCB: "Mark on PCB: Remove Mark", same price).

## Telemetry / PID tuning

`main.cpp` prints live PID tuning telemetry over `Serial` (115200 baud) in [Teleplot](https://teleplot.fr/)'s `>name:value` line format — install the Teleplot VS Code extension, point it at the board's serial port, and it plots `setpoint`, `measured`, `measuredFiltered`, `measuredAvg`, and `duty` live. This telemetry code (and the `TipTemperature::filteredValue()`/raw `value()` split it depends on) is marked `// TEMPORARY` in the source and intended to be removed once PID tuning is complete.

`TemperatureController`'s `Kp`/`Ki`/`Kd` gains are placeholders — this project has no characterized plant model (heater wattage, thermal mass, sensor lag), so they need on-device tuning: start with pure P, observe steady-state behavior, add `Ki` to remove steady-state error, and only add `Kd` afterward if overshoot still needs taming.
