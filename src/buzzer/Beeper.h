#pragma once

#include <stdint.h>

#include <etl/delegate.h>

// Non-blocking short beeps on the WIO Terminal's buzzer: beep(n) sounds n
// beeps of BEEP_MS with BEEP_MS gaps, without ever waiting. Each on/off
// edge is one step: startTimer_ arms a one-shot BEEP_MS software timer,
// whose expiry must end up calling onTimer() from task context (in
// main.cpp via a NotifyTask, like the other swTimer callbacks).
//
// Deliberately NOT tone(): the Seeed core's tone() runs on TC0, which
// LovyanGFX's WIO Terminal autodetect already uses for the LCD backlight PWM
// (Light_WioTerminal) - tone() resets TC0 and disables it when done,
// turning the backlight off. analogWrite() on WIO_BUZZER uses TCC0 (CH4)
// instead, which nothing else here touches.
//
// A beep() while a sequence is still running is ignored rather than
// restarting it, so a notification already queued by the running sequence's
// timer can't double-step a new one. Requests are rare (startup, a fault
// being latched) and short, so nothing important is lost.
class Beeper {
public:
  Beeper(uint32_t pin_, etl::delegate<void(void)> startTimer_) : pin(pin_), startTimer(startTimer_) {}

  void beep(uint8_t count) {
    if (stepsLeft != 0 || count == 0) {
      return;
    }
    // One step per edge after the first: on (now), off, on, ..., off.
    stepsLeft = static_cast<uint8_t>(2 * count - 1);
    setOn(true);
    startTimer();
  }

  void onTimer() {
    if (stepsLeft == 0) {
      return;
    }
    --stepsLeft;
    setOn(!on);
    if (stepsLeft != 0) {
      startTimer();
    }
  }

  // The one-shot timer period startTimer_ is expected to arm.
  static constexpr uint32_t BEEP_MS = 100;

private:
  void setOn(bool on_) {
    on = on_;
    analogWrite(pin, on ? DUTY_ON : 0);
  }

  // 50% duty: a square wave at the PWM frequency, as the startup beep always used.
  static constexpr int DUTY_ON = 128;

  uint32_t pin;
  etl::delegate<void(void)> startTimer;
  uint8_t stepsLeft = 0;
  bool on = false;
};
