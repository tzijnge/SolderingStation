#pragma once

#include <etl/task.h>

#include "TemperatureDisplay.h"
#include "TipTemperature.h"
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
class TipTemperatureDisplayTask : public etl::task {
public:
  TipTemperatureDisplayTask(TipTemperature &tipTemperature_, TemperatureDisplay &display_)
      : etl::task(TASK_PRIORITY_DISPLAY), tipTemperature(tipTemperature_), display(display_) {}

  uint32_t task_request_work() const override {
    return (tipTemperature.wholeDegrees() != lastDrawnValue) ? 1u : 0u;
  }

  void task_process_work() override {
    lastDrawnValue = tipTemperature.wholeDegrees();
    display.onTemperatureChanged(lastDrawnValue);
  }

private:
  TipTemperature &tipTemperature;
  TemperatureDisplay &display;
  int16_t lastDrawnValue = INT16_MIN; // guarantees the first round always draws
};
