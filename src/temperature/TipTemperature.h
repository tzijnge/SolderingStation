#pragma once

#include <stdint.h>

// Converts a raw 12-bit ADC sample (0..4095, 0..3.3V - see
// analogReadResolution(12) in main.cpp) into a Celsius tip temperature.
// Signal chain (nominal - the actual conversion is the measured
// calibration in onAdcSample()):
//   tip sensor (~16uV/*C) -> PCB amplifier (gain 68000/150) -> ADC (12-bit, 3.3V ref)
//
// Just stores the latest value - doesn't notify anyone. The (slow, SPI-
// bound) redraw is decoupled into TipTemperatureDisplayTask, which polls
// filteredValue() on its own schedule instead of onAdcSample() calling into
// the display inline - onAdcSample() runs as part of the ADC measurement
// cycle in main.cpp, and that cycle needs to stay fast and predictable
// (PWM resumes right after it) regardless of how long a redraw takes.
class TipTemperature {
public:
  void onAdcSample(uint16_t rawAdc) {
    // Calibrated against an FG-100 tip thermometer (room temperature ~17C),
    // 10 points from room temperature to 454C: cubic least-squares fit of
    // FG-100 reading vs raw ADC, residuals within +/-3C (mostly +/-1C).
    // The nominal chain above (0.1111 C/count through zero) read up to 25C
    // low - the real response has an offset and bends below ~200C. The
    // cubic is monotonic over the whole 0..4095 range (459.5C at 4095).
    //
    // No cold-junction compensation: the sensor measures tip minus ambient,
    // so readings shift by roughly (ambient - 17C).
    constexpr float C0 = 16.199f;
    constexpr float C1 = 0.14006f;
    constexpr float C2 = -1.6210e-5f;
    constexpr float C3 = 2.0614e-9f;

    rawAdc_ = rawAdc;
    float x = rawAdc;
    value_ = C0 + x * (C1 + x * (C2 + x * C3));

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

  // Full precision, unfiltered - kept for telemetry (the Teleplot
  // "measured" trace), where seeing the raw signal matters more than
  // smoothness.
  float value() const { return value_; }

  // Filtered - for the PID controller (TemperatureController), so sensor
  // noise doesn't directly translate into duty-cycle jitter via the P term.
  // Also what the display shows (TipTemperatureDisplayTask, which adds its
  // own hysteresis on top).
  float filteredValue() const { return filtered_; }

  // TEMPORARY: latest raw ADC sample, for calibration telemetry (fitting
  // the ADC-to-temperature conversion against an external tip thermometer).
  uint16_t rawAdc() const { return rawAdc_; }

private:
  static constexpr float FILTER_ALPHA = 0.3f;

  uint16_t rawAdc_ = 0;
  float value_ = 0.0f;
  float filtered_ = 0.0f;
};
