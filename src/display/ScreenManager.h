#pragma once

#include <LovyanGFX.hpp>
#include <etl/task.h>

#include "MainScreen.h"
#include "Screen.h"
#include "safety/FaultScreen.h"
#include "station/StationFsm.h"
#include "tasks/TaskPriority.h"

// Decides which full screen is showing and switches between them: Fault
// while StationFsm has a fault latched, Main otherwise. A switch clears the
// whole panel and has the new screen draw itself from scratch - screens are
// never overlaid on each other. Polls like the display tasks do, so the
// switch happens on its own turn in the scheduler, not inside whichever task
// latched or cleared the fault.
//
// The display tasks belonging to each screen check activeScreen themselves,
// so after a switch only the new screen's tasks draw. A display task may
// still draw onto the old screen earlier in the same scheduler round, before
// this task's turn - harmless, the clear below wipes it.
class ScreenManager : public etl::task {
public:
  ScreenManager(LGFX &tft_, const StationFsm &station_, ActiveScreen &activeScreen_, MainScreen &mainScreen_,
                FaultScreen &faultScreen_)
      : etl::task(TASK_PRIORITY_DISPLAY), tft(tft_), station(station_), activeScreen(activeScreen_),
        mainScreen(mainScreen_), faultScreen(faultScreen_) {}

  uint32_t task_request_work() const override { return (wanted() != activeScreen.get()) ? 1u : 0u; }

  void task_process_work() override {
    const Screen next = wanted();
    activeScreen.set(next);
    tft.fillScreen(TFT_BLACK);
    switch (next) {
    case Screen::Main:
      mainScreen.show();
      break;
    case Screen::Fault:
      faultScreen.show(station.fault());
      break;
    }
  }

private:
  Screen wanted() const { return (station.fault() != SafetyMonitor::Fault::None) ? Screen::Fault : Screen::Main; }

  LGFX &tft;
  const StationFsm &station;
  ActiveScreen &activeScreen;
  MainScreen &mainScreen;
  FaultScreen &faultScreen;
};
