#pragma once

#include <etl/task.h>

#include "TemperatureDisplay.h"
#include "TemperatureSetpoint.h"
#include "display/Screen.h"
#include "station/StationFsm.h"
#include "tasks/TaskPriority.h"

// Polls setpoint's value and the station's on/off state once per scheduler
// round and redraws only when either has changed since the last draw - same
// "only act on real change" idiom as TipTemperatureDisplayTask/
// CounterDisplayTask, decoupling the (slow, SPI-bound) redraw from whichever
// task (a button, the encoder, or the measurement cycle latching a fault)
// just changed it. Anything but On (Off, Fault) shows the "---" placeholder.
//
// Part of the Main screen: only draws while it's active. invalidate() forces
// a redraw - ScreenManager calls it after switching back to Main.
class SetpointDisplayTask : public etl::task {
public:
  SetpointDisplayTask(const TemperatureSetpoint &setpoint_, const StationFsm &station_, TemperatureDisplay &display_,
                      const ActiveScreen &activeScreen_)
      : etl::task(TASK_PRIORITY_DISPLAY), setpoint(setpoint_), station(station_), display(display_),
        activeScreen(activeScreen_) {}

  // The setpoint can never be INT16_MIN (clamped to 50..430), so this
  // always differs from the next value polled.
  void invalidate() { lastDrawnValue = INT16_MIN; }

  uint32_t task_request_work() const override {
    if (!activeScreen.is(Screen::Main)) {
      return 0u;
    }
    return (station.isOn() != lastOn || setpoint.value() != lastDrawnValue) ? 1u : 0u;
  }

  void task_process_work() override {
    lastOn = station.isOn();
    lastDrawnValue = setpoint.value();
    if (lastOn) {
      display.onTemperatureChanged(lastDrawnValue);
    } else {
      display.showOff();
    }
  }

private:
  const TemperatureSetpoint &setpoint;
  const StationFsm &station;
  TemperatureDisplay &display;
  const ActiveScreen &activeScreen;
  bool lastOn = true; // opposite of the real initial (off) state, forces the first round to draw
  int16_t lastDrawnValue = INT16_MIN;
};
