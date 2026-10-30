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
// Only reports whether a fault condition is present *right now*; the only
// state kept is the heater duty history of the thermal runaway and
// saturation checks (see checkRunaway() and checkSaturated()). Latching,
// acknowledging and holding the heater off are StationFsm's job (its Fault
// state), which also asks condition() to refuse an acknowledgement while
// the cause is still there.
//
// Further fault sources (sensor fault, supply voltage, tip detection -
// issues #3, #5, #8) add an enumerator and a check here.
class SafetyMonitor {
public:
  enum class Fault : uint8_t {
    None,
    OverTemperature,
    ThermalRunaway,
    HeaterSaturated,
  };

  // For fault messages (see FaultScreen). No default case on purpose:
  // -Wswitch then flags a new enumerator that hasn't been given a name here.
  static constexpr etl::string_view faultName(Fault fault) {
    switch (fault) {
    case Fault::None:
      return "None";
    case Fault::OverTemperature:
      return "OverTemperature";
    case Fault::ThermalRunaway:
      return "ThermalRunaway";
    case Fault::HeaterSaturated:
      return "HeaterSaturated";
    }
    return "?";
  }

  // Takes the unfiltered tip temperature (TipTemperature::value(), not
  // filteredValue()) - the EMA filter's lag must not delay this check - and
  // the PWM duty that was applied during the cycle that just ended, i.e. the
  // one this temperature is the response to. When several are present, the
  // first in this order wins: over-temperature, thermal runaway, saturation.
  Fault check(float tipTempC, uint8_t appliedDuty) {
    bool runaway = checkRunaway(tipTempC, appliedDuty);
    bool saturated = checkSaturated(appliedDuty);
    if (tipTempC > OVER_TEMPERATURE_LIMIT_C) {
      condition_ = Fault::OverTemperature;
    } else if (runaway) {
      condition_ = Fault::ThermalRunaway;
    } else if (saturated) {
      condition_ = Fault::HeaterSaturated;
    } else {
      condition_ = Fault::None;
    }
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
  // Thermal runaway: the heater is driven hard but the measured temperature
  // doesn't rise - a shorted sensor (reads raw ~0, i.e. ~16C, which looks
  // like a cold tip and gets full power), a sensor reading low, or a wiring
  // fault. Over-temperature can't catch that, and the PID would keep the
  // heater at full power indefinitely.
  //
  // Every cycle with at least RUNAWAY_DUTY_MIN extends the window, any
  // cycle below it resets it. After RUNAWAY_WINDOW_CYCLES the temperature
  // must have risen by at least RUNAWAY_MIN_RISE_C since the window started
  // (a healthy tip rises ~70C/s at full power), otherwise it's a fault;
  // after a good window the next one starts from the current temperature.
  //
  // Only armed while the tip reads below RUNAWAY_ARM_BELOW_C - an absolute
  // limit rather than a distance below the setpoint. A tip at working
  // temperature on a large ground plane can sit at full power for seconds
  // without rising; it shouldn't sag below this limit, while a failed sensor
  // reads far below it. A margin below the setpoint wouldn't work at the
  // low end: at the minimum setpoint (50C) a shorted sensor's ~16C is only
  // 34C below, yet the controller already runs at full power there.
  //
  // Once the heater is off (fault latched, duty 0) the window resets, so the
  // condition clears and the fault can be acknowledged - likewise for
  // checkSaturated().
  bool checkRunaway(float tipTempC, uint8_t appliedDuty) {
    if (appliedDuty < RUNAWAY_DUTY_MIN || tipTempC >= RUNAWAY_ARM_BELOW_C) {
      runawayCycles = 0;
      return false;
    }
    if (runawayCycles == 0) {
      runawayStartC = tipTempC;
    }
    if (++runawayCycles < RUNAWAY_WINDOW_CYCLES) {
      return false;
    }
    runawayCycles = 0;
    return tipTempC - runawayStartC < RUNAWAY_MIN_RISE_C;
  }

  // Heater saturation: the backstop for what checkRunaway() can't see - a
  // sensor that fails or loses contact while the tip is hot, so it keeps
  // reading above RUNAWAY_ARM_BELOW_C while the real temperature rises.
  // From temperature and duty alone that looks exactly like soldering a
  // large ground plane (full power, reading flat or slowly falling), so
  // this check only trips once the heater has been at RUNAWAY_DUTY_MIN or
  // above for SATURATED_CYCLES without a single cycle below it - far longer
  // than a heat-up from cold at full power (~6s to 430C) and, hopefully,
  // than real soldering needs. A false trip only costs an acknowledgement.
  bool checkSaturated(uint8_t appliedDuty) {
    if (appliedDuty < RUNAWAY_DUTY_MIN) {
      saturatedCycles = 0;
      return false;
    }
    if (saturatedCycles < SATURATED_CYCLES) {
      ++saturatedCycles;
    }
    return saturatedCycles >= SATURATED_CYCLES;
  }

  // Thresholds to be tuned on the device - see the device tests in issue #2.
  static constexpr uint8_t RUNAWAY_DUTY_MIN = 200;
  static constexpr uint8_t RUNAWAY_WINDOW_CYCLES = 30; // 3s at the 100ms measurement cycle
  static constexpr float RUNAWAY_MIN_RISE_C = 20.0f;
  static constexpr float RUNAWAY_ARM_BELOW_C = 150.0f;
  static constexpr uint16_t SATURATED_CYCLES = 300; // 30s at the 100ms measurement cycle

  Fault condition_ = Fault::None;
  uint8_t runawayCycles = 0;
  float runawayStartC = 0.0f;
  uint16_t saturatedCycles = 0;
};
