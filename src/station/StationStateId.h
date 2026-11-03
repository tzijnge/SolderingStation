#pragma once

#include <etl/fsm.h>

// StationFsm's states (see StationFsm.h). In a header of its own because the
// RPC interface uses it as an external enum (rpc/soldering_station.lrpc.yaml):
// the generated code includes this header before Arduino.h (see main.cpp), so
// it must stay free of Arduino.h and of anything that pulls it in.
namespace StationState
{

enum class StateId : etl::fsm_state_id_t
{
  Off,
  On,
  Fault,
};

} // namespace StationState
