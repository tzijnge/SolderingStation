#pragma once

#include <stdint.h>

// The user-facing target temperature (50..430 degrees C) - the only place
// it's tracked. Whether the heater actually runs is not this class's
// concern: that's StationFsm's state (On/Off/Fault), which also decides
// when diffSteps() gets called at all. Plain class, not an
// etl::delegate_observable like the old Counter: nothing needs push
// notification, SetpointDisplayTask and TemperatureController each poll
// value() on their own cadence instead (same idiom as TipTemperature).
//
// diffSteps() is called (via StationFsm) for producers (EncoderTask, button
// NotifyTasks) that don't know the current value, its clamp, or the step
// size - they just report "N steps", same shape as the old Counter::diff().
class TemperatureSetpoint {
public:
  void diffSteps(int16_t steps) {
    int32_t next = static_cast<int32_t>(value_) + steps * STEP_SIZE;
    value_ = static_cast<int16_t>((next < MIN) ? MIN : (next > MAX) ? MAX : next);
  }

  int16_t value() const { return value_; }

private:
  static constexpr int16_t MIN = 50;
  // Well below SafetyMonitor's 450C over-temperature cutoff, so normal
  // overshoot at the highest setpoint can't trip it.
  static constexpr int16_t MAX = 430;
  static constexpr int16_t STEP_SIZE = 5;

  int16_t value_ = MIN; // startup default, and what the very first "on" resumes to
};
