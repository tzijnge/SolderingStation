#pragma once

#include <stdint.h>

#include <etl/string_view.h>

// Detects fault conditions, checked once per measurement cycle (see
// measureAndResume() in main.cpp), before the new PWM duty is applied, so a
// detected fault forces the heater off within the same 100ms cycle it was
// seen in. Independent of TemperatureController/PID on purpose - this is
// the last line of defence against a controller bug, bad tuning or a
// corrupted setpoint, so it must not rely on any of them behaving.
//
// Stateless apart from the latest result: it only reports whether a fault
// condition is present *right now*. Latching, acknowledging and holding the
// heater off are StationFsm's job (its Fault state), which also asks
// condition() to refuse an acknowledgement while the cause is still there.
//
// Further fault sources (thermal runaway, sensor fault, supply voltage,
// tip detection - issues #2, #3, #5, #8) add an enumerator and a check
// here.
class SafetyMonitor {
public:
  enum class Fault : uint8_t {
    None,
    OverTemperature,
  };

  // For fault messages (see FaultScreen). No default case on purpose:
  // -Wswitch then flags a new enumerator that hasn't been given a name here.
  static constexpr etl::string_view faultName(Fault fault) {
    switch (fault) {
    case Fault::None:
      return "None";
    case Fault::OverTemperature:
      return "OverTemperature";
    }
    return "?";
  }

  // Takes the unfiltered tip temperature (TipTemperature::value(), not
  // filteredValue()) - the EMA filter's lag must not delay this check.
  Fault check(float tipTempC) {
    condition_ = (tipTempC > OVER_TEMPERATURE_LIMIT_C) ? Fault::OverTemperature : Fault::None;
    return condition_;
  }

  // Result of the latest check().
  Fault condition() const { return condition_; }

  // Above the highest setpoint (TemperatureSetpoint::MAX, 430), and below
  // the top of the measurable range: the calibration fit in TipTemperature
  // saturates at ~459.5C (raw ADC 4095), so anything higher can't be told
  // apart from 459.5 anyway - a limit above that could never trip. Public so
  // FaultScreen can show the actual limit in its message.
  static constexpr float OVER_TEMPERATURE_LIMIT_C = 450.0f;

private:
  Fault condition_ = Fault::None;
};
