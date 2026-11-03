#pragma once

#include <etl/delegate.h>
#include <soldering_station.hpp>

// RPC service "input": simulated knob and buttons. Posts nothing itself - it
// calls the same delegates as the real inputs (see main.cpp), so StationFsm
// can't tell the difference, and there's no path to the heater other than
// the one the knob has. That includes acknowledging a fault and switching
// the heater on.
//
// Like all etl::delegate use in this project, the delegates must reference
// named objects with program lifetime (see NotifyTask).
class InputService : public rpc::input_shim
{
public:
  InputService(etl::delegate<void(int16_t)> onRotate_, etl::delegate<void(void)> onPress_,
               etl::delegate<void(void)> onLeftButton_, etl::delegate<void(void)> onMiddleButton_,
               etl::delegate<void(void)> onRightButton_)
      : onRotate(onRotate_), onPress(onPress_), onLeftButton(onLeftButton_), onMiddleButton(onMiddleButton_),
        onRightButton(onRightButton_)
  {
  }

protected:
  void press() override { onPress(); }

  // No real input produces 0 steps, so it's ignored rather than letting it
  // switch the station on from Off without changing the setpoint.
  void rotate(int16_t steps) override
  {
    if (steps != 0)
    {
      onRotate(steps);
    }
  }

  // The button delegates notify the real buttons' tasks, so this takes the
  // same path as an actual button press from there on.
  void button(rpc::Button which) override
  {
    switch (which)
    {
    case rpc::Button::Left:
      onLeftButton();
      break;
    case rpc::Button::Middle:
      onMiddleButton();
      break;
    case rpc::Button::Right:
      onRightButton();
      break;
    }
  }

private:
  etl::delegate<void(int16_t)> onRotate;
  etl::delegate<void(void)> onPress;
  etl::delegate<void(void)> onLeftButton;
  etl::delegate<void(void)> onMiddleButton;
  etl::delegate<void(void)> onRightButton;
};
