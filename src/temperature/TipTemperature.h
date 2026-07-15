#pragma once

#include <stdint.h>

// Converts a raw 12-bit ADC sample (0..4095, 0..3.3V - see
// analogReadResolution(12) in main.cpp) into a Celsius tip temperature.
// Signal chain, worked backwards from the ADC reading to the sensor's own
// output voltage:
//   tip sensor (16uV/*C) -> PCB amplifier (gain 68000/150) -> ADC (12-bit, 3.3V ref)
//
// Just stores the latest value - doesn't notify anyone. The (slow, SPI-
// bound) redraw is decoupled into TipTemperatureDisplayTask, which polls
// wholeDegrees() on its own schedule instead of onAdcSample() calling into
// the display inline - onAdcSample() runs as part of the ADC measurement
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
    value_ = (vSensor * 1.0e6f) / SENSOR_MICROVOLTS_PER_DEGREE;

    // Exponential moving average - a much lighter filter than the 2-second
    // moving average used for the Teleplot trend line (see main.cpp): that
    // one is fine for a human eyeballing a chart, but its ~1s+ lag would
    // noticeably slow the control loop's reaction to real temperature
    // changes. FILTER_ALPHA trades noise rejection against lag - measured
    // raw sensor noise is about +/-2 degrees, which at Kp=10 was showing up
    // as visible duty jitter; tune this if that's still a problem, or if
    // the loop feels sluggish (lower/raise respectively).
    filtered_ = filtered_ + FILTER_ALPHA * (value_ - filtered_);
  }

  // Full precision, unfiltered - kept for display/telemetry (e.g. the
  // Teleplot "measured" trace) where seeing the raw signal matters more
  // than smoothness.
  float value() const { return value_; }

  // Filtered - for the PID controller (TemperatureController), so sensor
  // noise doesn't directly translate into duty-cycle jitter via the P term.
  float filteredValue() const { return filtered_; }

  // Rounded to nearest rather than truncated - a soldering iron tip is
  // never cold enough to need to worry about negative values here. For
  // display only (TipTemperatureDisplayTask) - font 7 shows whole degrees.
  int16_t wholeDegrees() const { return static_cast<int16_t>(value_ + 0.5f); }

private:
  static constexpr float FILTER_ALPHA = 0.3f;

  float value_ = 0.0f;
  float filtered_ = 0.0f;
};
