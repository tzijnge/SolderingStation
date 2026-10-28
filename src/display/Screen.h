#pragma once

#include <stdint.h>

// The full-screen layouts the display can show. Each owns the whole panel -
// switching clears it and draws the new layout from scratch (see
// ScreenManager), rather than overlaying one on another.
enum class Screen : uint8_t {
  Main,  // setpoint + tip temperature
  Fault, // latched fault: message, warning sign, tip temperature
};

// Which Screen is currently showing. Written only by ScreenManager; every
// display task checks it so it only ever draws on its own screen. A separate
// object (rather than asking ScreenManager) because ScreenManager in turn
// needs the display tasks to redraw them on a switch - this breaks that
// cycle.
class ActiveScreen {
public:
  bool is(Screen screen) const { return current == screen; }
  Screen get() const { return current; }
  void set(Screen screen) { current = screen; }

private:
  Screen current = Screen::Main; // setup() draws the Main screen's static parts at boot
};
