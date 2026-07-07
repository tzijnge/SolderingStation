#pragma once

#include <LovyanGFX.hpp>
#undef round // Arduino.h's round() macro (pulled in via LovyanGFX.hpp) breaks ETL's to_string float formatting
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

  TipTemperatureDisplay(LGFX &tft_, int32_t y_, uint16_t color_) : tft(tft_), y(y_), color(color_) {}

  // Draws the degree-C suffix once. The degree mark is a small solid ring
  // (filled circle with a smaller filled circle punched out of the
  // middle) rather than a font glyph or thin outline, for a bold, crisp
  // mark at any size. "C" uses LovyanGFX's bundled DejaVu72 (54px cap
  // height for 'C' - close to font 7's 48px digits without any custom
  // font generation).
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

    tft.setFont(&fonts::DejaVu72);
    // Bottom-align 'C' with the digits instead of top-aligning: DejaVu72's
    // TL_DATUM top edge sits 5px below the requested y (font-wide ascent is
    // 57px, from the backtick glyph, vs 'C's own 52px), so a plain y would
    // put 'C's top at y+5 and its bottom at y+59. Font 7's digits are a flat
    // 48px tall starting exactly at y (no TL_DATUM offset), so their bottom
    // is at y+48. Passing y-11 shifts 'C's bottom (y-11+59 = y+48) to match.
    tft.drawString("C", circleX + DEGREE_RADIUS + 6, y - 11);
    tft.setFont(&fonts::Font7);
  }

  void onTemperatureChanged(int16_t degreesC) {
    etl::string<16> text;
    etl::to_string(degreesC, text);

    tft.setTextColor(color, TFT_BLACK);
    tft.drawString(text.c_str(), 20, y); // uses the ambient font 7/size 1 set up in setup()
  }

private:
  static constexpr int32_t DEGREE_RADIUS = 6;

  LGFX &tft;
  int32_t y;
  uint16_t color;
};
