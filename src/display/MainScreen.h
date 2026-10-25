#pragma once

#include <LovyanGFX.hpp>

#include "temperature/SetpointDisplayTask.h"
#include "temperature/TemperatureDisplay.h"
#include "temperature/TipTemperatureDisplayTask.h"

// The normal screen: setpoint row on top, tip temperature row below (layout:
// see main.cpp). show() draws it onto a cleared panel - the static degree-C
// suffixes, then forces both rows' tasks to redraw their numbers on their
// next turn.
class MainScreen {
public:
  MainScreen(LGFX &tft_, TemperatureDisplay &setpointDisplay_, TemperatureDisplay &tipDisplay_,
             SetpointDisplayTask &setpointTask_, TipTemperatureDisplayTask &tipTask_)
      : tft(tft_), setpointDisplay(setpointDisplay_), tipDisplay(tipDisplay_), setpointTask(setpointTask_),
        tipTask(tipTask_) {}

  void show() {
    // Both rows rely on this ambient state (see setup() in main.cpp) - set
    // again here since other screens change it.
    tft.setFont(&fonts::Font7);
    tft.setTextPadding(TemperatureDisplay::NUMBER_PADDING);

    setpointDisplay.begin();
    tipDisplay.begin();
    setpointTask.invalidate();
    tipTask.invalidate();
  }

private:
  LGFX &tft;
  TemperatureDisplay &setpointDisplay;
  TemperatureDisplay &tipDisplay;
  SetpointDisplayTask &setpointTask;
  TipTemperatureDisplayTask &tipTask;
};
