#pragma once

#include <Arduino.h>
#include <SparkFun_Qwiic_Twist_Arduino_Library.h>
#include <etl/delegate.h>
#include <etl/task.h>

#include "TaskPriority.h"

// Interrupt-driven: the Twist's INT pad is wired to BCM21 (verified
// working on-device; BCM2 was tried first but shared a net with the
// Grove I2C bus despite looking independent in the Arduino pin table -
// see project notes). Confirmed against SparkFun's own
// Example8_Interrupts.ino: the line is open-drain, active low
// (INPUT_PULLUP + FALLING), and getCount() alone does NOT clear it -
// isMoved()/isPressed()/isClicked() do, each by clearing its own status
// bit in the Twist's STATUS register (see task_process_work() for why we
// use those instead of the library's blanket clearInterrupts()). The ISR
// can't safely do I2C, so it doesn't do the clearing itself.
//
// Two things had to be set in setup() for the interrupt to fire at all
// and to feel responsive: TWIST_ENABLE_INTS (the library never writes
// it) and setIntTimeout(0) (default firmware batches rotation into one
// interrupt ~250ms after you stop turning).
//
// task_request_work() reads the INT pin's level directly rather than
// counting edges from an ISR flag (contrast with ButtonTask). Reason:
// holding the Twist's own pushbutton down can make its firmware keep
// re-asserting "pressed", regenerating falling edges - with an ISR-set
// edge flag, that meant the flag got set again while it was stuck low
// (no further edges to re-trigger it), so EncoderTask silently stopped
// reporting work forever once that happened (buttons/display kept
// working fine, only the encoder went permanently unresponsive - not a
// scheduler-wide freeze, that was an earlier wrong guess). A level check
// is self-correcting: it naturally returns 0 the instant the line is
// actually released, so it keeps retrying for as long as needed instead
// of giving up after one edge. This also matches how SparkFun's own
// Example8_Interrupts.ino reads this pin - by polling
// digitalRead(pin) == LOW, not via attachInterrupt(). attachInterrupt()
// is still wired up (see onEncoderInterrupt in main.cpp), purely so
// __WFI() wakes promptly on this pin instead of waiting for the next 1ms
// SysTick tick; its callback doesn't need to do anything itself.
//
// EncoderTask knows nothing about Counter (or any other consumer) - it
// just reports raw hardware events through onDiff_/onReset_, two
// independently injected delegates. Like all etl::delegate use in this
// project, these must be named objects with program lifetime (e.g. a
// bound member-function delegate created in main.cpp), never an inline
// lambda literal - etl::delegate is a non-owning reference to its
// callable.
class EncoderTask : public etl::task {
public:
  EncoderTask(TWIST &twist_, uint32_t intPin_,
              etl::delegate<void(int16_t)> onDiff_,
              etl::delegate<void(void)> onReset_)
      : etl::task(TASK_PRIORITY_INPUT), twist(twist_), intPin(intPin_),
        onDiff(onDiff_), onReset(onReset_) {}

  void on_task_added() override { lastRawCount = twist.getCount(); }

  uint32_t task_request_work() const override {
    return (digitalRead(intPin) == LOW) ? 1u : 0u;
  }

  void task_process_work() override {
    // isMoved()/isPressed()/isClicked() each do their own I2C read of the
    // status byte, then their own I2C write of "that stale snapshot with
    // one bit cleared" - a structural race in the library itself, not
    // just the clearInterrupts() call removed earlier: if a new event
    // arrives on the device between one of these reads and its matching
    // write, that write overwrites the whole byte with the stale
    // snapshot, silently erasing the new event. isClicked() (latched once
    // on button release) only gets one narrow window to be caught, so it
    // occasionally lost the race. isPressed() is checked instead - reset
    // now happens on press (matching the WIO Terminal's own middle
    // button) and, since task_process_work() runs repeatedly for as long
    // as the INT line stays low during a hold, we get many chances to
    // observe it true rather than depending on one race-prone latch.
    twist.isMoved();
    int16_t rawCount = twist.getCount();
    // This board reports clockwise rotation as positive; negate here if
    // your unit behaves the other way.
    int16_t delta = rawCount - lastRawCount;
    lastRawCount = rawCount;

    bool pressed = twist.isPressed();
    twist.isClicked(); // unused, but must still be cleared or INT line stays low

    // A press takes precedence over any rotation reported in the same
    // call (matching the WIO Terminal's own middle button), so a
    // rotate-while-holding never fires a diff that the receiving end
    // would immediately have to overwrite with the reset.
    if (pressed) {
      onReset();
    } else if (delta != 0) {
      onDiff(delta);
    }
  }

private:
  TWIST &twist;
  uint32_t intPin;
  etl::delegate<void(int16_t)> onDiff;
  etl::delegate<void(void)> onReset;
  int16_t lastRawCount = 0;
};
