#pragma once

#include <Arduino.h>
#include <etl/task.h>

#include "TipTemperature.h"
#include "TipTemperatureDisplay.h"
#include "tasks/TaskPriority.h"

// Oscilloscope debug signal, temporarily bracketing the redraw itself (as
// opposed to main.cpp's MEASUREMENT_DEBUG_PIN, same physical pin, which
// has bracketed other spans earlier in this investigation) - gives a
// baseline measurement of the current (slow, per-pixel SPI) redraw cost
// to compare against, if a bulk-transfer version is done later.
constexpr uint32_t REDRAW_DEBUG_PIN = BCM20;

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
  TipTemperatureDisplayTask(TipTemperature &tipTemperature_, TipTemperatureDisplay &display_)
      : etl::task(TASK_PRIORITY_DISPLAY), tipTemperature(tipTemperature_), display(display_) {}

  uint32_t task_request_work() const override {
    return (tipTemperature.value() != lastDrawnValue) ? 1u : 0u;
  }

  void task_process_work() override {
    lastDrawnValue = tipTemperature.value();
    digitalWrite(REDRAW_DEBUG_PIN, HIGH);
    display.onTemperatureChanged(lastDrawnValue);
    digitalWrite(REDRAW_DEBUG_PIN, LOW);
  }

private:
  TipTemperature &tipTemperature;
  TipTemperatureDisplay &display;
  int16_t lastDrawnValue = INT16_MIN; // guarantees the first round always draws
};
