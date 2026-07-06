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
  CounterDisplay(TFT_eSPI &tft_, int32_t y_, uint16_t color_) : tft(tft_), y(y_), color(color_) {}

  void onCounterChanged(uint8_t value) {
    etl::string<16> text;
    etl::to_string(value, text);

    // Text colour is per-instance state on the shared tft object, so it
    // has to be set again right before drawing, every time - otherwise
    // whichever display drew last would leak its colour into this one.
    tft.setTextColor(color, TFT_BLACK);

    // drawString() only clears the padded box below, not the whole screen,
    // so the digits don't flash blank on every update.
    tft.drawString(text.c_str(), 20, y);
  }

private:
  TFT_eSPI &tft;
  int32_t y;
  uint16_t color;
};
