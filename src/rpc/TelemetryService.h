#pragma once

#include <Arduino.h>
#include <soldering_station.hpp>

#include "station/StationFsm.h"
#include "temperature/TemperatureSetpoint.h"
#include "temperature/TipTemperature.h"

// RPC service "telemetry": the "samples" stream, one sample per call of
// sendSample() (every 100 ms, from main.cpp's telemetry timer) while a client
// has started it. tools/telemetry.py forwards the samples to Teleplot.
//
// The stream also stops when the client closes the port (DTR drops, see
// sendSample()): a client killed without sending stop would otherwise leave
// it running, and the next lrpcc call would get a sample instead of its
// response.
class TelemetryService : public rpc::telemetry_shim
{
public:
  TelemetryService(Serial_ &serial_, const StationFsm &stationFsm_, const TemperatureSetpoint &temperatureSetpoint_,
                   const TipTemperature &tipTemperature_, const uint8_t &appliedDuty_)
      : serial(serial_), stationFsm(stationFsm_), temperatureSetpoint(temperatureSetpoint_),
        tipTemperature(tipTemperature_), appliedDuty(appliedDuty_)
  {
  }

  void sendSample()
  {
    // dtr() rather than Serial's operator bool, which delays 10 ms.
    if (!serial.dtr())
    {
      streaming = false;
    }
    if (!streaming)
    {
      return;
    }

    // 0 while not On (temperatureSetpoint still holds the real value, for
    // turning back on), so a plot clearly shows "no active setpoint".
    const int16_t setpoint = stationFsm.isOn() ? temperatureSetpoint.value() : 0;
    samples_response(setpoint, tipTemperature.value(), tipTemperature.filteredValue(), appliedDuty,
                     tipTemperature.rawAdc(), stationFsm.fault());
  }

protected:
  void samples() override { streaming = true; }
  void samples_stop() override { streaming = false; }

private:
  Serial_ &serial;
  const StationFsm &stationFsm;
  const TemperatureSetpoint &temperatureSetpoint;
  const TipTemperature &tipTemperature;
  const uint8_t &appliedDuty;
  bool streaming = false;
};
