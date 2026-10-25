#pragma once

#include <etl/task.h>

#include "TemperatureDisplay.h"
#include "TipTemperature.h"
#include "display/Screen.h"
#include "tasks/TaskPriority.h"

// Polls tipTemperature's value once per scheduler round and redraws only
// when it's changed since the last draw - the same "only act on real
// change" idiom this project's original Counter/CounterDisplay pair used
// before the delegate_observable refactor. Doing it this way (rather than
// TipTemperature notifying the display inline) means the (slow, SPI-
// bound - see project notes) redraw runs on its own turn in the
// scheduler, never blocking whichever task just produced the new value -
// specifically, main.cpp's ADC measurement cycle, which needs to resume
// PWM output right after sampling regardless of how long a redraw takes.
//
// Shows the filtered value (the same one the PID regulates on), with
// hysteresis: the shown number only changes once the temperature has moved
// more than HYSTERESIS_C away from it. Plain rounding - even of the
// filtered value - made a tip regulated right at e.g. 250 flicker between
// 249/250/251, since its filtered reading still wanders across the
// 249.5/250.5 rounding boundaries all the time.
//
// Used once per screen that shows the tip temperature (Main and Fault, each
// with its own TemperatureDisplay); only draws while its own screen is
// active. invalidate() forces a redraw - ScreenManager calls it after
// switching to that screen, since the cleared panel no longer shows
// lastDrawnValue.
class TipTemperatureDisplayTask : public etl::task {
public:
  TipTemperatureDisplayTask(const TipTemperature &tipTemperature_, TemperatureDisplay &display_,
                            const ActiveScreen &activeScreen_, Screen screen_)
      : etl::task(TASK_PRIORITY_DISPLAY), tipTemperature(tipTemperature_), display(display_),
        activeScreen(activeScreen_), screen(screen_) {}

  void invalidate() { lastDrawnValue = INT16_MIN; }

  uint32_t task_request_work() const override {
    if (!activeScreen.is(screen)) {
      return 0u;
    }
    float delta = tipTemperature.filteredValue() - lastDrawnValue;
    return (delta > HYSTERESIS_C || delta < -HYSTERESIS_C) ? 1u : 0u;
  }

  void task_process_work() override {
    // Rounded to nearest rather than truncated - a soldering iron tip is
    // never cold enough to need to worry about negative values here.
    lastDrawnValue = static_cast<int16_t>(tipTemperature.filteredValue() + 0.5f);
    display.onTemperatureChanged(lastDrawnValue);
  }

private:
  // Above 0.5, so a reading hovering around a rounding boundary can't flip
  // the shown value back and forth; below 1.0, so a real 1-degree change
  // still always shows up.
  static constexpr float HYSTERESIS_C = 0.75f;

  const TipTemperature &tipTemperature;
  TemperatureDisplay &display;
  const ActiveScreen &activeScreen;
  Screen screen;
  int16_t lastDrawnValue = INT16_MIN; // guarantees the first round always draws
};
