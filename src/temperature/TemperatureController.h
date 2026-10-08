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
// - i.e. bumpless transfer.
//
// Far below the setpoint (more than boostBandC), the PID is also held in
// MANUAL and the heater simply runs at full power. Left in AUTOMATIC, the
// output would be saturated at 255 anyway for almost the whole heat-up
// (Kp=20 saturates for any error above ~13 degrees), but the library would
// keep integrating meanwhile - its only anti-windup is clamping outputSum to
// the output limits - so the tip arrived at the setpoint with the I term
// alone at ~255, overshot ~5 degrees and took >10s to unwind. Instead, on
// entering the band, output is set to 0 right before SetMode(AUTOMATIC), so
// the integral starts from zero and only builds up where it's actually
// needed to find the holding duty.
//
// Kp/Ki/Kd are placeholders - this project has no characterized plant model
// (heater wattage, thermal mass, sensor lag), so there's no way to pick
// correct values without on-device tuning (see main.cpp for the starting
// values and tuning notes).
class TemperatureController {
public:
  TemperatureController(TemperatureSetpoint &setpoint_, double kp, double ki, double kd)
      : setpoint(setpoint_), boostBandC(255.0 / kp + BOOST_BAND_MARGIN_C),
        pid(&input, &output, &setpointValue, kp, ki, kd, DIRECT) {
    pid.SetOutputLimits(0, 255);
    pid.SetSampleTime(100);
    pid.SetMode(MANUAL); // starts off, matching TemperatureSetpoint's initial state
  }

  uint8_t compute(float measuredTempC) {
    if (!setpoint.isOn()) {
      output = 0;
      pid.SetMode(MANUAL);
      return 0;
    }

    // Set before any SetMode(AUTOMATIC) below: Initialize() seeds lastInput
    // (used by the derivative term) from input on that transition.
    setpointValue = setpoint.value();
    input = measuredTempC;

    if (setpointValue - input > boostBandC) {
      output = 255;
      pid.SetMode(MANUAL);
      return 255;
    }

    if (pid.GetMode() == MANUAL) {
      output = 0; // seeds outputSum (the integral) with 0 in Initialize()
      pid.SetMode(AUTOMATIC);
    }
    pid.Compute();
    return static_cast<uint8_t>(output);
  }

private:
  // The boost band is derived from Kp: 255/Kp is the width of the
  // proportional band, so the PID takes over while its own output would
  // still be saturated (no step down in duty at the handover), plus a small
  // margin. Re-tuning Kp therefore needs no matching change here.
  static constexpr double BOOST_BAND_MARGIN_C = 2.0;

  TemperatureSetpoint &setpoint;
  const double boostBandC;
  double input = 0;
  double output = 0;
  double setpointValue = 0;
  PID pid;
};
