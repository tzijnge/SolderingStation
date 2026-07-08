#pragma once

#include <stdint.h>

// The user-facing target temperature (50..450 degrees C) and the station's
// on/off state, both live here - the only place either is tracked. Plain
// class, not an etl::delegate_observable like the old Counter: nothing
// needs push notification anymore, SetpointDisplayTask and
// TemperatureController each poll value()/isOn() on their own cadence
// instead (same idiom as TipTemperature).
//
// diffSteps()/toggle() are called by producers (EncoderTask, button
// NotifyTasks) that don't know the current value, its clamp, or the step
// size - they just report "N steps" or "pressed", same shape as the old
// Counter::diff()/reset() they replace.
class TemperatureSetpoint {
public:
  // Turning the encoder or pressing +/- while off both turns the station on
  // and applies the step, in one action - clarified explicitly with the
  // user rather than assumed, since a diff while off could otherwise have
  // silently done nothing.
  void diffSteps(int16_t steps) {
    on_ = true;
    int32_t next = static_cast<int32_t>(value_) + steps * STEP_SIZE;
    value_ = static_cast<int16_t>((next < MIN) ? MIN : (next > MAX) ? MAX : next);
  }

  // Bound to both the middle top button and the Twist's own push (see
  // main.cpp) - same "press" event from two physical inputs, as it was for
  // the old Counter::reset(). value_ is left untouched while off, so
  // toggling back on always resumes the exact setpoint it had before.
  void toggle() { on_ = !on_; }

  bool isOn() const { return on_; }

  int16_t value() const { return value_; }

private:
  static constexpr int16_t MIN = 50;
  static constexpr int16_t MAX = 450;
  static constexpr int16_t STEP_SIZE = 5;

  int16_t value_ = MIN; // startup default, and what the very first "on" resumes to
  bool on_ = false;     // starts off
};
