#pragma once

#include <Arduino.h>
#include <etl/task.h>

#include "TaskPriority.h"

// Owns its PWM pin exclusively: mutation sites just touch counter, this is
// the only thing that ever calls analogWrite(). Mirrors DisplayTask's
// "redraw only if changed" pattern, but since counter is a full uint8_t
// there's no unused sentinel value to borrow for "not yet written" - the
// first write is instead forced explicitly in on_task_added().
class PwmTask : public etl::task {
public:
  PwmTask(uint32_t pin_, const uint8_t &counter_)
      : etl::task(TASK_PRIORITY_INPUT), pin(pin_), counter(counter_) {}

  void on_task_added() override {
    pinMode(pin, OUTPUT);
    analogWrite(pin, counter);
    lastWritten = counter;
  }

  uint32_t task_request_work() const override {
    return (counter != lastWritten) ? 1u : 0u;
  }

  void task_process_work() override {
    analogWrite(pin, counter);
    lastWritten = counter;
  }

private:
  uint32_t pin;
  const uint8_t &counter;
  uint8_t lastWritten = 0;
};
