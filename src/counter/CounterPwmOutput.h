#pragma once

#include <Arduino.h>

// Drives a PWM pin whenever notified of a new counter value. Not a message
// router - onCounterChanged() is just a plain method, registered with
// Counter as an etl::delegate observer (see main.cpp). begin() must be
// called from setup() (not the constructor) since pinMode() needs the
// Arduino core to have run its own init() first, which hasn't happened yet
// at global static-init time.
class CounterPwmOutput {
public:
  explicit CounterPwmOutput(uint32_t pin_) : pin(pin_) {}

  void begin() { pinMode(pin, OUTPUT); }

  void onCounterChanged(uint8_t value) { analogWrite(pin, value); }

private:
  uint32_t pin;
};
