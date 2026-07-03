#include <Arduino.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <SparkFun_Qwiic_Twist_Arduino_Library.h>
#include <etl/function.h>
#include <etl/scheduler.h>

#include "tasks/ButtonTask.h"
#include "tasks/DisplayTask.h"
#include "tasks/EncoderTask.h"
#include "tasks/PwmTask.h"

TFT_eSPI tft;
TWIST twist;

// Seeed_GFX/TFT_eSPI has no enum for its built-in fonts, just numeric IDs
// (1, 2, 4, 6, 7, 8) documented in the library README.
constexpr uint8_t SEVEN_SEGMENT_FONT = 7;

uint8_t counter = 0;

EncoderTask encoderTask(twist, counter, BCM21);
PwmTask pwmTask(BCM23, counter);
DisplayTask displayTask(tft, counter);

// counter is global, so these don't need to capture it - keeping them
// captureless also means they'd be convertible to a function pointer, but
// they're still passed by name below, never as literals (see ButtonTask).
auto decrementCounter = []() {
  if (counter > 0) --counter;
};
auto resetCounter = []() { counter = 0; };
auto incrementCounter = []() {
  if (counter < 255) ++counter;
};
ButtonTask leftButtonTask(decrementCounter);
ButtonTask middleButtonTask(resetCounter);
ButtonTask rightButtonTask(incrementCounter);

// "Single" (not "multiple"): processes at most one unit of work per task
// per round before moving to the next, so a task whose task_request_work()
// stays true for an extended period (e.g. EncoderTask while the Twist's
// button is held) can't starve the tasks after it in the list, like
// DisplayTask, from ever getting a turn.
etl::scheduler<etl::scheduler_policy_sequential_single, 6> scheduler;

// Runs whenever a full scheduler pass finds no task with work. yield() is
// a no-op on this build (USBCON, not TinyUSB - USB is interrupt-driven and
// doesn't need it), kept only in case some future library overrides that
// weak hook. __WFI() sleeps the CPU until the next interrupt (a button
// press, an encoder interrupt, or the 1ms SysTick tick).
void onIdle() {
  yield();
  __WFI();
}

etl::function_fv<onIdle> idleCallback;

// WIO_KEY_A/B/C are the three top buttons (variant.h). Confirmed on-device:
// left = WIO_KEY_C, middle = WIO_KEY_B, right = WIO_KEY_A.
void onLeftButton() { leftButtonTask.notifyPressed(); }
void onMiddleButton() { middleButtonTask.notifyPressed(); }
void onRightButton() { rightButtonTask.notifyPressed(); }

// Twist's INT pad wired to BCM21 (40-pin header, physical position 40).
void onEncoderInterrupt() {
  // Intentionally empty: EncoderTask::task_request_work() reads the pin
  // level directly. This callback exists only so the attached interrupt
  // wakes the CPU from __WFI() sleep promptly.
}

void setup() {
  Wire.begin();
  Wire.setClock(400000); // fast-mode I2C, keeps polling latency low

  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);

  if (!twist.begin()) {
    tft.println("Qwiic Twist not found!");
    while (true) {
      delay(1000);
    }
  }

  // The Twist library never writes TWIST_ENABLE_INTS (register 0x04,
  // bit0 = encoder interrupt enable, bit1 = button interrupt enable - see
  // enableInterruptEncoderBit/enableInterruptButtonBit in the library's
  // header) - no public method touches it, so it's whatever the module's
  // firmware defaulted to. Force both bits on directly over I2C.
  Wire.beginTransmission(QWIIC_TWIST_ADDR);
  Wire.write(0x04); // TWIST_ENABLE_INTS
  Wire.write(0x03); // bit0 encoder int enable, bit1 button int enable
  Wire.endTransmission();

  // Default firmware waits ~250ms after turning stops before firing the
  // movement interrupt (coalesces rapid turning into one interrupt), which
  // made the display update in laggy bursts. Try to get closer to
  // real-time by minimizing that wait.
  twist.setIntTimeout(0);

  // Buttons pull the pin to GND when pressed, so use the internal
  // pull-up and trigger on the falling edge.
  pinMode(WIO_KEY_A, INPUT_PULLUP);
  pinMode(WIO_KEY_B, INPUT_PULLUP);
  pinMode(WIO_KEY_C, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(WIO_KEY_A), onRightButton, FALLING);
  attachInterrupt(digitalPinToInterrupt(WIO_KEY_B), onMiddleButton, FALLING);
  attachInterrupt(digitalPinToInterrupt(WIO_KEY_C), onLeftButton, FALLING);

  // Twist's INT line is open-drain, active low - same pull-up/FALLING
  // pattern as the buttons.
  pinMode(BCM21, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BCM21), onEncoderInterrupt, FALLING);

  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(SEVEN_SEGMENT_FONT); // 0-9, ':', '-', '.'
  tft.setTextSize(1);
  tft.setTextPadding(260); // fixed-width clear box, big enough for e.g. "-32768"

  scheduler.add_task(encoderTask);
  scheduler.add_task(leftButtonTask);
  scheduler.add_task(middleButtonTask);
  scheduler.add_task(rightButtonTask);
  scheduler.add_task(pwmTask);
  scheduler.add_task(displayTask);
  scheduler.set_idle_callback(idleCallback);
}

void loop() {
  // Blocks here running the cooperative task loop for as long as the
  // scheduler is running (i.e. forever, unless something calls
  // scheduler.exit_scheduler()).
  scheduler.start();
}
