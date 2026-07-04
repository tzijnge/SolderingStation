#pragma once

#include <stdint.h>

#include <etl/delegate_observable.h>

// The only place the counter's value and its 0..255 clamp live. diff()/
// reset() are called by producers (EncoderTask, ButtonTask) that don't
// know the current value or its valid range - they just report what
// happened. Whenever the clamped value actually changes, notifies every
// registered observer (see etl::delegate_observable::add_observer) with
// the new value - currently CounterDisplay and CounterPwmOutput.
class Counter : public etl::delegate_observable<uint8_t, 2> {
public:
  // Unconditionally notifies observers of the current value - call once
  // from setup() after registering them, since nothing else triggers that
  // first display draw/PWM write for a value that starts unchanged at 0.
  void begin() { notify_observers(value); }

  void diff(int16_t delta) {
    int16_t next = static_cast<int16_t>(value) + delta;
    setValue(static_cast<uint8_t>((next < 0) ? 0 : (next > 255) ? 255 : next));
  }

  void reset() { setValue(0); }

private:
  void setValue(uint8_t next) {
    if (next != value) {
      value = next;
      notify_observers(value);
    }
  }

  uint8_t value = 0;
};
