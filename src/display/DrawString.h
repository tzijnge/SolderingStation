#pragma once

#include <LovyanGFX.hpp>
#include <etl/string.h>
#include <etl/string_view.h>

// LovyanGFX's drawString() only takes a NUL-terminated const char* (or an
// Arduino String), and an etl::string_view isn't guaranteed to be
// NUL-terminated - so the text is copied into an etl::string for its
// c_str(). Text longer than MAX_LENGTH is truncated; MAX_LENGTH is well past
// what fits on the 320px wide screen in any font used here.
inline size_t drawString(LGFX &tft, etl::string_view text, int32_t x, int32_t y) {
  constexpr size_t MAX_LENGTH = 64;
  etl::string<MAX_LENGTH> terminated(text.data(), text.size());
  return tft.drawString(terminated.c_str(), x, y);
}
