#pragma once

#include <etl/task.h>

#include "Counter.h"
#include "CounterDisplay.h"
#include "tasks/TaskPriority.h"

// Polls counter's value once per scheduler round and redraws only when
// it's changed since the last draw - keeps the (slow, SPI-bound - see
// project notes) redraw off whichever task just changed the counter (a
// button press, an encoder turn, or a software timer tick), the same
// reasoning as TipTemperatureDisplayTask. One class, reused for both
// counter/display and secondsCounter/secondsDisplay.
class CounterDisplayTask : public etl::task {
public:
  CounterDisplayTask(Counter &counter_, CounterDisplay &display_)
      : etl::task(TASK_PRIORITY_DISPLAY), counter(counter_), display(display_) {}

  uint32_t task_request_work() const override {
    return (counter.value() != lastDrawnValue) ? 1u : 0u;
  }

  void task_process_work() override {
    lastDrawnValue = counter.value();
    display.onCounterChanged(lastDrawnValue);
  }

private:
  Counter &counter;
  CounterDisplay &display;
  uint8_t lastDrawnValue = 0xFF; // 0xFF != 0 (Counter's initial value), forces first-round draw
};
