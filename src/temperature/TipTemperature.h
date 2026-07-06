#pragma once

#include <stdint.h>

#include <etl/delegate_observable.h>

// Converts a raw 12-bit ADC sample (0..4095, 0..3.3V - see
// analogReadResolution(12) in main.cpp) into a whole-degree Celsius tip
// temperature and notifies observers. Signal chain, worked backwards from
// the ADC reading to the sensor's own output voltage:
//   tip sensor (16uV/*C) -> PCB amplifier (gain 68000/150) -> ADC (12-bit, 3.3V ref)
class TipTemperature : public etl::delegate_observable<int16_t, 1> {
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
    int16_t next = static_cast<int16_t>(tempC + 0.5f);

    // Sampled every 100ms, but the rounded value is usually stable for
    // several samples in a row - only notify (and so only redraw) when it
    // actually changes, or the display would flicker on every sample.
    if (next != lastValue) {
      lastValue = next;
      notify_observers(next);
    }
  }

private:
  int16_t lastValue = INT16_MIN; // guarantees the first sample always notifies
};
