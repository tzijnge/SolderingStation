#pragma once

#include <LovyanGFX.hpp>
#include <etl/string.h>
#include <etl/string_stream.h>

#include "SafetyMonitor.h"
#include "display/DrawString.h"
#include "images/WarningSign.h"
#include "temperature/TemperatureDisplay.h"
#include "temperature/TipTemperatureDisplayTask.h"

// Full screen shown while a fault is latched (see ScreenManager), top to
// bottom:
//   y   4..30   the fault message, red
//   y  46..140  the warning sign bitmap (images/WarningSign.h, generated from
//               assets/warning_sign.svg)
//   y 152..206  the live tip temperature (centered TemperatureDisplay, kept
//               up to date by its own TipTemperatureDisplayTask), so the user
//               can watch the tip cool down before acknowledging
//   y 220..236  "Press knob to reset" hint
//
// show() draws the static parts onto a cleared panel; only the temperature
// changes afterwards.
class FaultScreen {
public:
  FaultScreen(LGFX &tft_, TemperatureDisplay &tipDisplay_, TipTemperatureDisplayTask &tipTask_)
      : tft(tft_), tipDisplay(tipDisplay_), tipTask(tipTask_) {}

  void show(SafetyMonitor::Fault fault) {
    tft.setTextPadding(0);
    tft.setTextDatum(TC_DATUM);

    auto message = faultText(fault);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    // Font4 (26px) normally; falls back to Font2 (16px) for a message too
    // long to fit the screen width in Font4.
    tft.setFont(&fonts::Font4);
    if (tft.textWidth(message.c_str()) > tft.width() - 2 * SIDE_MARGIN) {
      tft.setFont(&fonts::Font2);
    }
    drawString(tft, message, tft.width() / 2, MESSAGE_Y);

    WarningSign::draw(tft, (tft.width() - WarningSign::WIDTH) / 2, SIGN_Y);

    tft.setFont(&fonts::Font2);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString("Press knob to reset", tft.width() / 2, HINT_Y);

    // TemperatureDisplay relies on this ambient state (see setup() in
    // main.cpp).
    tft.setFont(&fonts::Font7);
    tft.setTextPadding(TemperatureDisplay::NUMBER_PADDING);
    tipDisplay.begin();
    tipTask.invalidate();
  }

  // The row the live tip temperature is drawn at - for its TemperatureDisplay
  // in main.cpp. 6px below the sign, since the degree-C 'C' sticks up 6px
  // above the digits (see the layout comment in main.cpp).
  static constexpr int32_t TEMPERATURE_Y = 158;

private:
  static constexpr int32_t SIDE_MARGIN = 4;
  static constexpr int32_t MESSAGE_Y = 4;
  static constexpr int32_t SIGN_Y = 46;
  static constexpr int32_t HINT_Y = 220;

  // Built at runtime rather than a constexpr literal, so the over-temperature
  // message always shows the actual SafetyMonitor limit. The limit is shown
  // in whole degrees (it's a float only for the comparison); "C" rather than
  // a degree sign, since the fonts used here are plain ASCII. No "FAULT"
  // prefix - the warning sign says that.
  static etl::string<40> faultText(SafetyMonitor::Fault fault) {
    etl::string<40> text;
    etl::string_stream stream(text);
    switch (fault) {
    case SafetyMonitor::Fault::OverTemperature:
      stream << "Tip above " << static_cast<int>(SafetyMonitor::OVER_TEMPERATURE_LIMIT_C) << "C limit";
      break;
    case SafetyMonitor::Fault::ThermalRunaway:
      stream << "Tip not heating up";
      break;
    case SafetyMonitor::Fault::HeaterSaturated:
      stream << "Heater at full power too long";
      break;
    default:
      // A fault without its own message yet - at least name it (the numeric
      // value too, in case faultName() doesn't know it either).
      stream << "Fault: " << SafetyMonitor::faultName(fault) << " (" << static_cast<unsigned>(fault) << ")";
      break;
    }
    return text;
  }

  LGFX &tft;
  TemperatureDisplay &tipDisplay;
  TipTemperatureDisplayTask &tipTask;
};
