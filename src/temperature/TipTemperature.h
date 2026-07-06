#pragma once

#include <stdint.h>

// Converts a raw 12-bit ADC sample (0..4095, 0..3.3V - see
// analogReadResolution(12) in main.cpp) into a whole-degree Celsius tip
// temperature. Signal chain, worked backwards from the ADC reading to the
// sensor's own output voltage:
//   tip sensor (16uV/*C) -> PCB amplifier (gain 68000/150) -> ADC (12-bit, 3.3V ref)
//
// Just stores the latest value - doesn't notify anyone. The (slow, SPI-
// bound) redraw is decoupled into TipTemperatureDisplayTask, which polls
// value() on its own schedule instead of onAdcSample() calling into the
// display inline - onAdcSample() runs as part of the ADC measurement
// cycle in main.cpp, and that cycle needs to stay fast and predictable
// (PWM resumes right after it) regardless of how long a redraw takes.
class TipTemperature {
public:
  void onAdcSample(uint16_t rawAdc) {
    constexpr float VREF = 3.3f;
    constexpr float ADC_MAX = 4095.0f; // 2^12 - 1
    constexpr float AMPLIFIER_GAIN = 68000.0f / 150.0f;
    constexpr float SENSOR_MICROVOLTS_PER_DEGREE = 16.0f;

    float vAdc = (rawAdc * VREF) / ADC_MAX;
    float vSensor = vAdc / AMPLIFIER_GAIN;
    float tempC = (vSensor * 1.0e6f) / SENSOR_MICROVOLTS_PER_DEGREE;

    // Rounds to nearest rather than truncating - a soldering iron tip is
    // never cold enough to need to worry about negative values here.
    value_ = static_cast<int16_t>(tempC + 0.5f);
  }

  int16_t value() const { return value_; }

private:
  int16_t value_ = 0;
};
