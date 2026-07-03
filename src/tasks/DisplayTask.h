#pragma once

#include <TFT_eSPI.h>
#undef round // Arduino.h's round() macro (pulled in via TFT_eSPI.h) breaks ETL's to_string float formatting
#include <etl/string.h>
#include <etl/task.h>
#include <etl/to_string.h>

#include "TaskPriority.h"

// Redraws the counter on the TFT whenever it has changed since the last draw.
class DisplayTask : public etl::task {
public:
  DisplayTask(TFT_eSPI &tft_, const uint8_t &counter_)
      : etl::task(TASK_PRIORITY_DISPLAY), tft(tft_), counter(counter_) {}

  uint32_t task_request_work() const override {
    return (counter != lastDrawnCounter) ? 1u : 0u;
  }

  void task_process_work() override {
    etl::string<16> text;
    etl::to_string(counter, text);

    // drawString() only clears the padded box below, not the whole screen,
    // so the digits don't flash blank on every update.
    tft.drawString(text.c_str(), 20, 90);
    lastDrawnCounter = counter;
  }

private:
  TFT_eSPI &tft;
  const uint8_t &counter;
  uint8_t lastDrawnCounter = 0xFF; // force first draw (0xFF != 0)
};
