#pragma once

#include <Arduino.h>
#include <etl/task.h>
#include <soldering_station.hpp>

#include "tasks/TaskPriority.h"

// The LotusRPC server (generated from rpc/soldering_station.lrpc.yaml during
// the build, see tools/lrpc_generate.py) on the USB serial port, and the
// scheduler task that feeds it. RPC handlers therefore run in task context,
// where StationFsm events may be posted.
//
// Nothing gets lost while this task waits for its turn: the USB CDC driver
// holds two 64-byte packets, and when they're full the hardware NAKs the PC
// until there's room again.
//
// The server finds message boundaries only by the length byte, so a stray
// byte misaligns all following messages until a reboot - accepted, since only
// the PC tools talk to this port (see #24).
class RpcServer : public rpc::soldering_station, public etl::task
{
public:
  explicit RpcServer(Serial_ &serial_) : etl::task(TASK_PRIORITY_INPUT), serial(serial_) {}

  uint32_t task_request_work() const override { return serial.available() > 0 ? 1u : 0u; }

  // At most one USB packet's worth per turn, so a burst of input can't hold
  // up the other tasks. read() rather than readBytes(), which waits up to
  // the stream timeout (1 s) for more.
  void task_process_work() override
  {
    for (int i = 0; i < 64 && serial.available() > 0; ++i)
    {
      lrpcReceive(static_cast<uint8_t>(serial.read()));
    }
  }

private:
  // Nothing is sent while no client has the port open (DTR, see
  // TelemetryService). With the port open but not being read, write() gives
  // up after 70 ms per packet; a frame of up to 64 bytes (a telemetry sample
  // is 17) is one packet, so it's either sent whole or dropped whole and the
  // client stays in sync.
  void lrpcTransmit(lrpc::span<const uint8_t> bytes) override
  {
    if (serial.dtr())
    {
      serial.write(bytes.data(), bytes.size());
    }
  }

  Serial_ &serial;
};
