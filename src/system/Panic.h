#pragma once

#include <etl/string_view.h>

// Stops the station on an unrecoverable programming error (e.g. an event a
// StationFsm state has no handler for): heater off, the two lines of text on
// a red error screen and over serial, then waits for a press of the encoder
// button and reboots. Never returns - nothing after a programming error can
// be trusted to keep running, so it doesn't go back to the scheduler at all.
//
// Defined in main.cpp, which owns the hardware it needs (tft, twist,
// pwmOutput, swTimer).
[[noreturn]] void panic(etl::string_view title, etl::string_view detail);
