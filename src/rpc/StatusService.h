#pragma once

#include <soldering_station.hpp>

#include "station/StationFsm.h"
#include "temperature/TemperatureSetpoint.h"
#include "temperature/TipTemperature.h"

// RPC service "status": read-only queries of the station's state. Members
// aren't named after what they hold (setpoint, ...) because the shim's
// functions already are.
class StatusService : public rpc::status_shim
{
public:
  StatusService(const StationFsm &stationFsm_, const TemperatureSetpoint &temperatureSetpoint_,
                const TipTemperature &tipTemperature_, const uint8_t &appliedDuty_)
      : stationFsm(stationFsm_), temperatureSetpoint(temperatureSetpoint_), tipTemperature(tipTemperature_),
        appliedDuty(appliedDuty_)
  {
  }

protected:
  rpc::StateId state() override { return stationFsm.state(); }
  rpc::Fault fault() override { return stationFsm.fault(); }
  int16_t setpoint() override { return temperatureSetpoint.value(); }
  rpc::Temperatures tip_temperature() override { return {tipTemperature.value(), tipTemperature.filteredValue()}; }
  uint8_t duty() override { return appliedDuty; }
  rpc::Status all() override
  {
    return {state(), fault(), setpoint(), tipTemperature.value(), tipTemperature.filteredValue(), duty()};
  }

private:
  const StationFsm &stationFsm;
  const TemperatureSetpoint &temperatureSetpoint;
  const TipTemperature &tipTemperature;
  const uint8_t &appliedDuty;
};
