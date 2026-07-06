#pragma once

#include <TFT_eSPI.h>
#undef round // Arduino.h's round() macro (pulled in via TFT_eSPI.h) breaks ETL's to_string float formatting
#include "MicroGroteskBold64.h" // single 'C' glyph, see that file for provenance/license
#include <etl/string.h>
#include <etl/to_string.h>

// Redraws the tip temperature's number whenever notified of a new value;
// the degree-C suffix next to it is static (font 7 has no degree sign or
// letters at all - see project notes - and the suffix never changes
// regardless of the number), so it's drawn once via begin() instead of on
// every update, avoiding needless redraw flicker.
class TipTemperatureDisplay {
public:
  // The fixed-width background-clear box main.cpp sets via
  // tft.setTextPadding(TipTemperatureDisplay::NUMBER_PADDING) - shared so
  // the degree-C suffix's fixed position (see begin()) and the number's
  // own clearing width can never drift out of sync with each other again.
  static constexpr int32_t NUMBER_PADDING = 110;

  TipTemperatureDisplay(TFT_eSPI &tft_, int32_t y_, uint16_t color_) : tft(tft_), y(y_), color(color_) {}

  // Draws the degree-C suffix once. The degree mark is a small solid ring
  // (filled circle with a smaller filled circle punched out of the
  // middle) rather than a font glyph or thin outline, for a bold, crisp
  // mark at any size. "C" uses MicroGroteskBold64 (50px cap height at its
  // native size, no integer-scaling needed) - a single glyph converted
  // from Micro Grotesk specifically to get close to font 7's 48px digits
  // without the size mismatch or scaling artifacts of the bundled fonts.
  //
  // Positioned just past NUMBER_PADDING - the same fixed-width clear box
  // main.cpp sets via tft.setTextPadding(TipTemperatureDisplay::NUMBER_PADDING),
  // so the number's own background-clearing can never reach into the
  // suffix (that mismatch, an independently-guessed number here vs the
  // actual padding in main.cpp, is what caused the circle to get its left
  // half erased before).
  void begin() {
    int32_t circleX = 20 + NUMBER_PADDING + 10 + DEGREE_RADIUS;
    int32_t circleY = y + DEGREE_RADIUS + 4;

    tft.setTextColor(color, TFT_BLACK);
    tft.fillCircle(circleX, circleY, DEGREE_RADIUS, color);
    tft.fillCircle(circleX, circleY, DEGREE_RADIUS - 3, TFT_BLACK);

    tft.setFreeFont(&MicroGroteskBold64);
    tft.drawString("C", circleX + DEGREE_RADIUS + 6, y);
    tft.setTextFont(7);
  }

  void onTemperatureChanged(int16_t degreesC) {
    etl::string<16> text;
    etl::to_string(degreesC, text);

    tft.setTextColor(color, TFT_BLACK);
    tft.drawString(text.c_str(), 20, y); // uses the ambient font 7/size 1 set up in setup()
  }

private:
  static constexpr int32_t DEGREE_RADIUS = 6;

  TFT_eSPI &tft;
  int32_t y;
  uint16_t color;
};
