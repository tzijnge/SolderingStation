# SolderingStation

[WIO Terminal](https://wiki.seeedstudio.com/Wio_Terminal_Intro/) adaptation of the [Elektor Platino soldering station](https://www.elektormagazine.com/magazine/elektor-201507/27978)

## Development environment

### Tooling

- [Visual Studio Code](https://code.visualstudio.com/)
- [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode) for VS Code (recommended in `.vscode/extensions.json`)
  - Installs its own toolchain (GCC ARM, `pio` CLI, etc.) on first use — no separate Arduino IDE or ARM toolchain install needed

### Hardware target

- [Seeed WIO Terminal](https://wiki.seeedstudio.com/Wio_Terminal_Intro/) (SAMD51, Cortex-M4F)
- `atmelsam` platform, `board = seeed_wio_terminal`, `framework = arduino` (see `platformio.ini`)

### Libraries

Declared in `platformio.ini` under `lib_deps`, fetched automatically by PlatformIO on build:

- [Seeed_Arduino_LCD](https://github.com/Seeed-Studio/Seeed_Arduino_LCD) — drives the 2.4" ILI9341 LCD (included as `TFT_eSPI.h`, class `TFT_eSPI`)
- [Seeed_Arduino_FS](https://github.com/Seeed-Studio/Seeed_Arduino_FS) — required alongside `Seeed_Arduino_LCD`; the LCD library fails to build without it even when filesystem features aren't used
- [SparkFun_Qwiic_Twist_Arduino_Library](https://github.com/sparkfun/SparkFun_Qwiic_Twist_Arduino_Library) — driver for the Qwiic Twist rotary encoder, connected via a Grove-to-Qwiic cable on the WIO Terminal's Grove I2C port
- [ETL (Embedded Template Library)](https://github.com/ETLCPP/etl) — used throughout for containers/utilities in this interrupt-driven design

### Building and uploading

Build and upload using the PlatformIO sidebar in VS Code (project/build/upload icons in the PlatformIO toolbar), or the equivalent `pio run` / `pio run -t upload` commands from a PlatformIO Core CLI.

### Known build gotchas

- `Arduino.h` `#define`s `round(x)` as a macro, which breaks ETL's `etl::to_string`. Add `#undef round` right after `#include <Arduino.h>` and before any `etl/*` includes in any file that includes both.
