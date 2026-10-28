#pragma once

#include <etl/message.h>
#include <etl/string_view.h>

#include "safety/SafetyMonitor.h"

// Events driving StationFsm. Producers (EncoderTask, the button NotifyTasks,
// the measurement cycle in main.cpp) only post these - what an event means
// depends on the station's current state, which only StationFsm knows.
// Posted from scheduler task context only, never from an ISR: etl::fsm
// isn't interrupt-safe, and the button/encoder ISRs already defer to tasks.
namespace StationEvent {

enum : etl::message_id_t {
  ROTATE,
  PRESS,
  FAULT_DETECTED,
};

// Setpoint +/- by a number of steps (encoder detents, or +-1 per button).
struct Rotate : public etl::message<ROTATE> {
  explicit Rotate(int16_t steps_) : steps(steps_) {}
  int16_t steps;
};

// Knob push or the middle button.
struct Press : public etl::message<PRESS> {};

// Posted every measurement cycle in which SafetyMonitor sees a fault
// condition, not just on the first one - StationFsm's Fault state ignores
// the repeats, so the producer doesn't need to track what it already sent.
struct FaultDetected : public etl::message<FAULT_DETECTED> {
  explicit FaultDetected(SafetyMonitor::Fault fault_) : fault(fault_) {}
  SafetyMonitor::Fault fault;
};

// For the unhandled-event panic screen (see StationFsm.h). Add new events
// here too, or they show up as "?" (the numeric id is shown either way).
constexpr etl::string_view eventName(etl::message_id_t id) {
  switch (id) {
  case ROTATE:
    return "Rotate";
  case PRESS:
    return "Press";
  case FAULT_DETECTED:
    return "FaultDetected";
  default:
    return "Unhandled event";
  }
}

} // namespace StationEvent
