#pragma once

#include <etl/atomic.h>
#include <etl/delegate.h>
#include <etl/task.h>

#include "TaskPriority.h"

// A pending count is bumped by notify() - called from any interrupt
// context (a button EXTI interrupt, a SysTick-driven software timer tick,
// etc.) - and the scheduler drains it, invoking onNotify_ once per
// pending notification. NotifyTask knows nothing about what notify()
// means or who's calling it - onNotify_ is injected per instance, so one
// generic class serves buttons and timer callbacks alike.
//
// etl::delegate is a non-owning reference to its callable - it just stores
// a pointer to it - so onNotify_ must be a named object with program
// lifetime (e.g. a file-scope lambda), never a lambda literal passed
// directly here. A bare temporary compiles fine (even when captureless)
// but leaves onNotify pointing at a destroyed object: verified by hand
// against ETL's actual delegate_cpp11.h overload set, since the deleted
// rvalue-lambda constructor only guards lambdas that *aren't* convertible
// to a function pointer, not this case.
class NotifyTask : public etl::task {
public:
  explicit NotifyTask(etl::delegate<void(void)> onNotify_)
      : etl::task(TASK_PRIORITY_INPUT), onNotify(onNotify_) {}

  void notify() { pending.fetch_add(1); }

  uint32_t task_request_work() const override { return pending.load(); }

  void task_process_work() override {
    pending.fetch_sub(1);
    onNotify();
  }

private:
  etl::delegate<void(void)> onNotify;
  etl::atomic<uint32_t> pending{0};
};
