#pragma once

#include <etl/task.h>

#include "TemperatureDisplay.h"
#include "TemperatureSetpoint.h"
#include "tasks/TaskPriority.h"

// Polls setpoint's value and on/off state once per scheduler round and
// redraws only when either has changed since the last draw - same "only act
// on real change" idiom as TipTemperatureDisplayTask/CounterDisplayTask,
// decoupling the (slow, SPI-bound) redraw from whichever task (a button, the
// encoder, or the PID controller reading isOn()) just changed the setpoint.
class SetpointDisplayTask : public etl::task {
public:
  SetpointDisplayTask(TemperatureSetpoint &setpoint_, TemperatureDisplay &display_)
      : etl::task(TASK_PRIORITY_DISPLAY), setpoint(setpoint_), display(display_) {}

  uint32_t task_request_work() const override {
    return (setpoint.isOn() != lastOn || setpoint.value() != lastDrawnValue) ? 1u : 0u;
  }

  void task_process_work() override {
    lastOn = setpoint.isOn();
    lastDrawnValue = setpoint.value();
    if (lastOn) {
      display.onTemperatureChanged(lastDrawnValue);
    } else {
      display.showOff();
    }
  }

private:
  TemperatureSetpoint &setpoint;
  TemperatureDisplay &display;
  bool lastOn = true; // opposite of the real initial (off) state, forces the first round to draw
  int16_t lastDrawnValue = INT16_MIN;
};
