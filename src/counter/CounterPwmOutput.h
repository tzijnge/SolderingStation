#pragma once

#include <Arduino.h>

// Drives a PWM pin whenever given a new duty value. onCounterChanged() is
// just a plain method - currently called directly by main.cpp's
// TemperatureController once per ADC measurement cycle, not via observer
// registration. begin() must be called from setup() (not the constructor)
// since pinMode() needs the Arduino core to have run its own init() first,
// which hasn't happened yet at global static-init time.
class CounterPwmOutput {
public:
  explicit CounterPwmOutput(uint32_t pin_) : pin(pin_) {}

  void begin() { pinMode(pin, OUTPUT); }

  void onCounterChanged(uint8_t value) {
    lastValue = value;
    // Suppressed while paused so a counter change mid-measurement can't
    // sneak the output back on before resume() - see pause()/resume().
    if (!paused) {
      analogWrite(pin, value);
    }
  }

  // Temporarily forces the output off (without forgetting the last real
  // value) so resume() can restore it - used to quiet PWM switching noise
  // during a tip-temperature ADC measurement (see main.cpp).
  void pause() {
    paused = true;
    analogWrite(pin, 0);
  }

  // Restarts the PWM waveform's phase, not just its duty cycle - forces
  // the underlying timer's counter back to a known instant instead of
  // letting it keep free-running from wherever it happened to be
  // (analogWrite() only ever updates the compare register, never the
  // counter itself). Not needed for the heating behaviour - the tip
  // doesn't care about PWM phase - but it means the scope, triggered on
  // the measurement-cycle debug pin, shows a static waveform instead of
  // one that visually "walks" cycle to cycle.
  //
  // BCM23/A2 (the only pin this class is ever constructed with in this
  // project) is driven by TC1 channel 1 in 8-bit mode (see the project's
  // PWM-frequency notes) - CTRLBSET.CMD = RETRIGGER is that peripheral's
  // hardware command for an immediate counter reset. If this class is
  // ever reused for a different pin, this would need to target whichever
  // TC/TCC that pin actually uses instead.
  void resume() {
    paused = false;
    analogWrite(pin, lastValue);

    while (TC1->COUNT8.SYNCBUSY.bit.CTRLB) {
    }
    TC1->COUNT8.CTRLBSET.bit.CMD = TC_CTRLBSET_CMD_RETRIGGER_Val;
  }

private:
  uint32_t pin;
  uint8_t lastValue = 0;
  bool paused = false;
};
