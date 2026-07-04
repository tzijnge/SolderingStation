#pragma once

#include <TFT_eSPI.h>
#undef round // Arduino.h's round() macro (pulled in via TFT_eSPI.h) breaks ETL's to_string float formatting
#include <etl/string.h>
#include <etl/to_string.h>

// Redraws the counter on the TFT whenever notified of a new value. Not a
// message router - onCounterChanged() is just a plain method, registered
// with Counter as an etl::delegate observer (see main.cpp).
class CounterDisplay {
public:
  explicit CounterDisplay(TFT_eSPI &tft_) : tft(tft_) {}

  void onCounterChanged(uint8_t value) {
    etl::string<16> text;
    etl::to_string(value, text);

    // drawString() only clears the padded box below, not the whole screen,
    // so the digits don't flash blank on every update.
    tft.drawString(text.c_str(), 20, 90);
  }

private:
  TFT_eSPI &tft;
};
