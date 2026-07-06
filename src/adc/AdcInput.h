#pragma once

#include <Arduino.h>
#include <etl/delegate_observable.h>

// Reads pin_ and notifies observers of the raw ADC value (0..4095 at
// 12-bit resolution - see analogReadResolution(12) in setup()) whenever
// sample() is called. Doesn't decide when to sample itself - driven
// externally (see the 100ms timer wired up in main.cpp), same as Counter
// doesn't decide when it's diffed/reset.
class AdcInput : public etl::delegate_observable<uint16_t, 1> {
public:
  explicit AdcInput(uint32_t pin_) : pin(pin_) {}

  void sample() { notify_observers(analogRead(pin)); }

private:
  uint32_t pin;
};
