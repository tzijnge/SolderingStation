#pragma once

#include <stdint.h>

#include <etl/delegate_observable.h>

// The only place the counter's value and its 0..255 clamp live. diff()/
// reset() are called by producers (EncoderTask, ButtonTask) that don't
// know the current value or its valid range - they just report what
// happened. Whenever the clamped value actually changes, notifies every
// registered observer (see etl::delegate_observable::add_observer) -
// currently just CounterPwmOutput, which needs to react immediately (no
// SPI cost, unlike a display redraw). CounterDisplayTask polls value()
// directly instead of observing, for the same reason
// TipTemperatureDisplayTask polls TipTemperature - see project notes.
class Counter : public etl::delegate_observable<uint8_t, 1> {
public:
  // Unconditionally notifies observers of the current value - call once
  // from setup() after registering them, since nothing else triggers
  // that first PWM write for a value that starts unchanged at 0.
  void begin() { notify_observers(value_); }

  void diff(int16_t delta) {
    int16_t next = static_cast<int16_t>(value_) + delta;
    setValue(static_cast<uint8_t>((next < 0) ? 0 : (next > 255) ? 255 : next));
  }

  void reset() { setValue(0); }

  uint8_t value() const { return value_; }

private:
  void setValue(uint8_t next) {
    if (next != value_) {
      value_ = next;
      notify_observers(value_);
    }
  }

  uint8_t value_ = 0;
};
