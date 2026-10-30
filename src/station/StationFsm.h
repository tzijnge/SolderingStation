#pragma once

#include <etl/delegate.h>
#include <etl/fsm.h>
#include <etl/string.h>
#include <etl/string_stream.h>
#include <etl/string_view.h>

#include "StationEvents.h"
#include "safety/SafetyMonitor.h"
#include "system/Panic.h"
#include "temperature/TemperatureSetpoint.h"

// The station's top-level state - the one place that decides what an input
// means and whether the heater may run. State diagram: docs/state-machine.md
// (keep it in sync with every change here).
//
// Rotate while On just adjusts the setpoint; Rotate while faulted is
// ignored, so turning the knob can't switch the heater back on. A fault
// always lands in Off after acknowledgement, never straight back in On -
// heating again takes a deliberate second press.
//
// Entering Fault calls onFaultLatched_ (two beeps, in main.cpp) - while
// soldering the user is likely watching the joint, not the screen, so a
// fault must be audible too.
//
// Future states (standby #16, the settings menu #12) slot in here as more
// etl::fsm_state types - the menu likely as a sub-state via etl::hfsm, since
// the heater keeps its On/Off state while the menu is open.
class StationFsm;

namespace StationState {

enum class StateId : etl::fsm_state_id_t {
  Off,
  On,
  Fault,
};

// etl::fsm_state takes, and its handlers return, a plain fsm_state_id_t -
// a scoped enum doesn't convert implicitly, so this does it explicitly.
// constexpr so it also works as the fsm_state template argument below.
constexpr etl::fsm_state_id_t id(StateId state) { return static_cast<etl::fsm_state_id_t>(state); }

constexpr etl::string_view stateName(StateId state) {
  switch (state) {
  case StateId::Off:
    return "Off";
  case StateId::On:
    return "On";
  case StateId::Fault:
    return "Fault";
  }
  return "?";
}

// etl::fsm_state calls on_event_unknown() for any message type the state
// doesn't list. Every state lists every event it's meant to receive, so
// getting here means an event was posted that some state wasn't written to
// handle - a programming error, not a runtime condition. Each state's
// on_event_unknown() forwards here, which panics (heater off, error screen,
// reboot on encoder press - see system/Panic.h) rather than silently
// dropping the event.
[[noreturn]] inline void unhandledEvent(StateId state, const etl::imessage &event) {
  etl::string<64> detail;
  etl::string_stream stream(detail);
  // message_id_t is a uint8_t - widened so it's streamed as a number, not a character.
  const auto eventId = static_cast<unsigned>(event.get_message_id());
  stream << StationEvent::eventName(event.get_message_id()) << " (id " << eventId << ") in state " << stateName(state);
  panic("Unhandled FSM event", detail);
}

using namespace StationEvent;

class Off : public etl::fsm_state<StationFsm, Off, id(StateId::Off), Rotate, Press, FaultDetected> {
public:
  etl::fsm_state_id_t on_event(const Rotate &event);
  etl::fsm_state_id_t on_event(const Press &event);
  etl::fsm_state_id_t on_event(const FaultDetected &event);
  etl::fsm_state_id_t on_event_unknown(const etl::imessage &event) { unhandledEvent(StateId::Off, event); }
};

class On : public etl::fsm_state<StationFsm, On, id(StateId::On), Rotate, Press, FaultDetected> {
public:
  etl::fsm_state_id_t on_event(const Rotate &event);
  etl::fsm_state_id_t on_event(const Press &event);
  etl::fsm_state_id_t on_event(const FaultDetected &event);
  etl::fsm_state_id_t on_event_unknown(const etl::imessage &event) { unhandledEvent(StateId::On, event); }
};

class Fault : public etl::fsm_state<StationFsm, Fault, id(StateId::Fault), Rotate, Press, FaultDetected> {
public:
  etl::fsm_state_id_t on_event(const Rotate &event);
  etl::fsm_state_id_t on_event(const Press &event);
  etl::fsm_state_id_t on_event(const FaultDetected &event);
  etl::fsm_state_id_t on_event_unknown(const etl::imessage &event) { unhandledEvent(StateId::Fault, event); }
  etl::fsm_state_id_t on_enter_state() override;
  void on_exit_state() override;
};

} // namespace StationState

class StationFsm : public etl::fsm {
public:
  // onFaultLatched_ is a non-owning etl::delegate: its callable must be a
  // named object with program lifetime (see NotifyTask).
  StationFsm(TemperatureSetpoint &setpoint_, const SafetyMonitor &safetyMonitor_,
             etl::delegate<void(void)> onFaultLatched_)
      : etl::fsm(ROUTER_ID), setpoint(setpoint_), safetyMonitor(safetyMonitor_), onFaultLatched(onFaultLatched_) {
    set_states(states);
  }

  // The heater may only run in On - TemperatureController and the
  // measurement cycle check this every cycle (see main.cpp).
  bool isOn() const { return get_state_id() == StationState::id(StationState::StateId::On); }

  // The latched fault while in Fault, None otherwise - for ScreenManager,
  // FaultScreen and telemetry.
  SafetyMonitor::Fault fault() const { return latchedFault; }

private:
  friend class StationState::Off;
  friend class StationState::On;
  friend class StationState::Fault;

  static constexpr etl::message_router_id_t ROUTER_ID = 0;

  etl::fsm_state_pack<StationState::Off, StationState::On, StationState::Fault> states;

  TemperatureSetpoint &setpoint;
  const SafetyMonitor &safetyMonitor;
  etl::delegate<void(void)> onFaultLatched;
  SafetyMonitor::Fault latchedFault = SafetyMonitor::Fault::None;
};

namespace StationState {

// Turning the encoder or pressing +/- while off both turns the station on
// and applies the step, in one action - clarified explicitly with the user
// rather than assumed, since a diff while off could otherwise have silently
// done nothing.
inline etl::fsm_state_id_t Off::on_event(const Rotate &event) {
  get_fsm_context().setpoint.diffSteps(event.steps);
  return id(StateId::On);
}

inline etl::fsm_state_id_t Off::on_event(const Press &) { return id(StateId::On); }

inline etl::fsm_state_id_t Off::on_event(const FaultDetected &event) {
  get_fsm_context().latchedFault = event.fault;
  return id(StateId::Fault);
}

inline etl::fsm_state_id_t On::on_event(const Rotate &event) {
  get_fsm_context().setpoint.diffSteps(event.steps);
  return No_State_Change;
}

inline etl::fsm_state_id_t On::on_event(const Press &) { return id(StateId::Off); }

inline etl::fsm_state_id_t On::on_event(const FaultDetected &event) {
  get_fsm_context().latchedFault = event.fault;
  return id(StateId::Fault);
}

inline etl::fsm_state_id_t Fault::on_event(const Rotate &) { return No_State_Change; }

// Acknowledging while the condition is still present is refused - it'd
// only re-latch on the very next measurement cycle anyway, but refusing
// here keeps the fault (and its message) steady instead of flickering off.
inline etl::fsm_state_id_t Fault::on_event(const Press &) {
  return get_fsm_context().safetyMonitor.condition() == SafetyMonitor::Fault::None ? id(StateId::Off) : No_State_Change;
}

// The first fault wins, so the display keeps showing the original cause
// rather than whatever followed from it.
inline etl::fsm_state_id_t Fault::on_event(const FaultDetected &) { return No_State_Change; }

// Only reached from Off/On on a FaultDetected, so it runs once per latched
// fault, never for the repeats Fault itself ignores.
inline etl::fsm_state_id_t Fault::on_enter_state() {
  get_fsm_context().onFaultLatched();
  return No_State_Change;
}

inline void Fault::on_exit_state() { get_fsm_context().latchedFault = SafetyMonitor::Fault::None; }

} // namespace StationState
