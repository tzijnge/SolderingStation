#pragma once

#include <PID_v1.h>

#include "TemperatureSetpoint.h"

// Turns (setpoint, measured tip temperature) into a PWM duty (0..255) via
// br3ttb/Arduino-PID-Library. compute() is meant to be called once per ADC
// measurement cycle (see main.cpp's measureAndResume(), ~every 100ms) -
// SetSampleTime() below matches that cadence.
//
// While the station is off, output is forced to 0 and the PID is held in
// MANUAL mode rather than left AUTOMATIC with a stale/growing integral term.
// Verified against the library's actual source (PID_v1.cpp): SetMode(AUTOMATIC)
// calls Initialize() on a MANUAL->AUTOMATIC transition, which seeds the
// internal integral accumulator (outputSum) from the current *myOutput value
// - i.e. bumpless transfer. Since output is 0 while off, turning back on
// always resumes from a clean, zero integral term instead of whatever would
// otherwise have wound up while idle.
//
// Kp/Ki/Kd are placeholders - this project has no characterized plant model
// (heater wattage, thermal mass, sensor lag), so there's no way to pick
// correct values without on-device tuning (see main.cpp for the starting
// values and tuning notes).
class TemperatureController {
public:
  TemperatureController(TemperatureSetpoint &setpoint_, double kp, double ki, double kd)
      : setpoint(setpoint_), pid(&input, &output, &setpointValue, kp, ki, kd, DIRECT) {
    pid.SetOutputLimits(0, 255);
    pid.SetSampleTime(100);
    pid.SetMode(MANUAL); // starts off, matching TemperatureSetpoint's initial state
  }

  uint8_t compute(int16_t measuredTempC) {
    if (!setpoint.isOn()) {
      output = 0;
      pid.SetMode(MANUAL);
      return 0;
    }

    pid.SetMode(AUTOMATIC); // no-op once already automatic; bumpless-inits outputSum=0 on the off->on edge
    setpointValue = setpoint.value();
    input = measuredTempC;
    pid.Compute();
    return static_cast<uint8_t>(output);
  }

private:
  TemperatureSetpoint &setpoint;
  double input = 0;
  double output = 0;
  double setpointValue = 0;
  PID pid;
};
