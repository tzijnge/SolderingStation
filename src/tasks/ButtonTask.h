#pragma once

#include <etl/atomic.h>
#include <etl/delegate.h>
#include <etl/task.h>

#include "TaskPriority.h"

// One button: an ISR (notifyPressed()) increments an atomic pending count,
// and the scheduler drains it, invoking onPress_ once per press.
// ButtonTask knows nothing about Counter or any other consumer - onPress_
// is injected per instance (e.g. a lambda that calls counter.diff(-1) in
// main.cpp), so one generic class serves all three buttons.
//
// etl::delegate is a non-owning reference to its callable - it just stores
// a pointer to it - so onPress_ must be a named object with program
// lifetime (e.g. a file-scope lambda), never a lambda literal passed
// directly here. A bare temporary compiles fine (even when captureless)
// but leaves onPress pointing at a destroyed object: verified by hand
// against ETL's actual delegate_cpp11.h overload set, since the deleted
// rvalue-lambda constructor only guards lambdas that *aren't* convertible
// to a function pointer, not this case.
class ButtonTask : public etl::task {
public:
  explicit ButtonTask(etl::delegate<void(void)> onPress_)
      : etl::task(TASK_PRIORITY_INPUT), onPress(onPress_) {}

  void notifyPressed() { pending.fetch_add(1); }

  uint32_t task_request_work() const override { return pending.load(); }

  void task_process_work() override {
    pending.fetch_sub(1);
    onPress();
  }

private:
  etl::delegate<void(void)> onPress;
  etl::atomic<uint32_t> pending{0};
};
