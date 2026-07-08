#pragma once

#include <LovyanGFX.hpp>
#undef round // Arduino.h's round() macro (pulled in via LovyanGFX.hpp) breaks ETL's to_string float formatting
#include <etl/string.h>
#include <etl/to_string.h>

// Redraws a temperature number whenever notified of a new value; the
// degree-C suffix next to it is static (font 7 has no degree sign or
// letters at all - see project notes - and the suffix never changes
// regardless of the number), so it's drawn once via begin() instead of on
// every update, avoiding needless redraw flicker.
//
// Not tip-specific despite the sizing being originally tuned for it - this
// is reused for both the measured tip temperature and the user-set
// temperature setpoint (see main.cpp), which is why the class and its
// methods are named generically rather than "Tip...".
class TemperatureDisplay {
public:
  // The fixed-width background-clear box main.cpp sets via
  // tft.setTextPadding(TemperatureDisplay::NUMBER_PADDING) - shared so
  // the degree-C suffix's fixed position (see begin()) and the number's
  // own clearing width can never drift out of sync with each other again.
  static constexpr int32_t NUMBER_PADDING = 110;

  TemperatureDisplay(LGFX &tft_, int32_t y_, uint16_t color_) : tft(tft_), y(y_), color(color_) {}

  // Draws the degree-C suffix once. The degree mark is a small solid ring
  // (filled circle with a smaller filled circle punched out of the
  // middle) rather than a font glyph or thin outline, for a bold, crisp
  // mark at any size. "C" uses LovyanGFX's bundled DejaVu72 (54px cap
  // height for 'C' - close to font 7's 48px digits without any custom
  // font generation).
  //
  // Positioned just past NUMBER_PADDING - the same fixed-width clear box
  // main.cpp sets via tft.setTextPadding(TemperatureDisplay::NUMBER_PADDING),
  // so the number's own background-clearing can never reach into the
  // suffix (that mismatch, an independently-guessed number here vs the
  // actual padding in main.cpp, is what caused the circle to get its left
  // half erased before).
  void begin() {
    int32_t circleX = LEFT_MARGIN + NUMBER_PADDING + 10 + DEGREE_RADIUS;
    int32_t circleY = y + DEGREE_RADIUS + 4;

    tft.setTextColor(color, TFT_BLACK);
    tft.fillCircle(circleX, circleY, DEGREE_RADIUS, color);
    tft.fillCircle(circleX, circleY, DEGREE_RADIUS - 3, TFT_BLACK);

    tft.setTextDatum(TL_DATUM); // the number itself uses TR_DATUM - see onTemperatureChanged()/showOff()
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
    drawRightAligned(text.c_str());
  }

  // Placeholder for "no active setpoint" (station off) - font 7 only has
  // 0-9, ':', '-', '.' (no letters), so "OFF" isn't renderable; three dashes
  // is the closest equivalent within that character set.
  void showOff() {
    tft.setTextColor(color, TFT_BLACK);
    drawRightAligned("---");
  }

private:
  static constexpr int32_t LEFT_MARGIN = 20;
  static constexpr int32_t DEGREE_RADIUS = 6;

  // Right-aligned within the same [LEFT_MARGIN, LEFT_MARGIN+NUMBER_PADDING]
  // box the old left-aligned number used to clear - TR_DATUM's padding
  // fills leftward from the given x (see LGFXBase::draw_string()), so
  // passing the box's right edge keeps the exact same clear footprint,
  // just with digits growing left instead of right as the value's width
  // changes. The degree-C suffix (begin(), above) doesn't move - it's
  // positioned off NUMBER_PADDING, not off the number's own alignment.
  void drawRightAligned(const char *text) {
    tft.setTextDatum(TR_DATUM);
    tft.drawString(text, LEFT_MARGIN + NUMBER_PADDING, y);
  }

  LGFX &tft;
  int32_t y;
  uint16_t color;
};
