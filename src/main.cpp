#include <Arduino.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <SparkFun_Qwiic_Twist_Arduino_Library.h>
#include <etl/callback_timer_interrupt.h>
#include <etl/delegate.h>
#include <etl/function.h>
#include <etl/scheduler.h>

#include "counter/Counter.h"
#include "counter/CounterDisplay.h"
#include "counter/CounterPwmOutput.h"
#include "tasks/EncoderTask.h"
#include "tasks/NotifyTask.h"

TFT_eSPI tft;
TWIST twist;

// Seeed_GFX/TFT_eSPI has no enum for its built-in fonts, just numeric IDs
// (1, 2, 4, 6, 7, 8) documented in the library README.
constexpr uint8_t SEVEN_SEGMENT_FONT = 7;

CounterDisplay display(tft, 90);
CounterPwmOutput pwmOutput(BCM23);

// The only place the counter's value and its 0..255 clamp live. Neither
// producer below knows this type exists beyond what's bound into their
// delegates (see counter.add_observer() calls in setup() for the other
// direction, display/pwmOutput -> counter).
Counter counter;

// onDiff/onReset bind directly to Counter's own methods - no wrapper
// needed since the delegate signatures already match exactly. EncoderTask
// itself never sees the Counter type.
EncoderTask encoderTask(twist, BCM21,
                        etl::delegate<void(int16_t)>::create<Counter, &Counter::diff>(counter),
                        etl::delegate<void(void)>::create<Counter, &Counter::reset>(counter));

// Each button needs a fixed argument (+-1) or no argument at all baked in,
// which a plain bound-member delegate can't express, hence these small
// lambdas rather than binding straight to Counter::diff/reset. Like all
// etl::delegate use in this project, they must be named objects with
// program lifetime, never inline lambda literals (see NotifyTask for why).
auto decrementCounter = []() { counter.diff(-1); };
auto resetCounter = []() { counter.reset(); };
auto incrementCounter = []() { counter.diff(1); };
NotifyTask leftButtonTask(decrementCounter);
NotifyTask middleButtonTask(resetCounter);
NotifyTask rightButtonTask(incrementCounter);

// A second, independent counter driven by 1-second and 30-second software
// timers instead of hardware input, just to prove the timer plumbing out -
// see secondsTimer/sysTickHook() below for how they actually tick.
CounterDisplay secondsDisplay(tft, 150); // font 7 is 48px tall, so 90+48 clears the first display with a small margin
Counter secondsCounter;
auto incrementSeconds = []() { secondsCounter.diff(1); };
auto resetSeconds = []() { secondsCounter.reset(); };
NotifyTask secondsTask(incrementSeconds);
NotifyTask resetSecondsTask(resetSeconds);

// Guards register_timer()/start()/stop() (called from setup(), i.e. normal
// code) against a concurrent SysTick interrupt calling tick() mid-update -
// tick() itself doesn't need guarding since it *is* that interrupt.
struct SysTickGuard {
  SysTickGuard() { NVIC_DisableIRQ(SysTick_IRQn); }
  ~SysTickGuard() { NVIC_EnableIRQ(SysTick_IRQn); }
};

// A 1-second repeating tick and a 30-second repeating reset. tick() is
// called with a 1ms count from sysTickHook() below, so periods registered
// on this are in milliseconds.
etl::callback_timer_interrupt<2, SysTickGuard> secondsTimer;

// "Single" (not "multiple"): processes at most one unit of work per task
// per round before moving to the next, so a task whose task_request_work()
// stays true for an extended period (e.g. EncoderTask while the Twist's
// button is held) can't starve the tasks after it in the list. counter,
// display and pwmOutput aren't scheduler tasks at all - each update
// happens synchronously, inline, as part of the producing task's
// task_process_work() call.
etl::scheduler<etl::scheduler_policy_sequential_single, 6> scheduler;

// Runs whenever a full scheduler pass finds no task with work. yield() is
// a no-op on this build (USBCON, not TinyUSB - USB is interrupt-driven and
// doesn't need it), kept only in case some future library overrides that
// weak hook. __WFI() sleeps the CPU until the next interrupt (a button
// press, an encoder interrupt, the 1ms SysTick tick, or ...).
void onIdle() {
  yield();
  __WFI();
}

etl::function_fv<onIdle> idleCallback;

// WIO_KEY_A/B/C are the three top buttons (variant.h). Confirmed on-device:
// left = WIO_KEY_C, middle = WIO_KEY_B, right = WIO_KEY_A.
void onLeftButton() { leftButtonTask.notify(); }
void onMiddleButton() { middleButtonTask.notify(); }
void onRightButton() { rightButtonTask.notify(); }

// Twist's INT pad wired to BCM21 (40-pin header, physical position 40).
void onEncoderInterrupt() {
  // Intentionally empty: EncoderTask::task_request_work() reads the pin
  // level directly. This callback exists only so the attached interrupt
  // wakes the CPU from __WFI() sleep promptly.
}

// Called from the real SysTick_Handler, before the Arduino core's own
// default handler (see hooks.c) - this is what actually drives
// secondsTimer, replacing millis()/delay()-based polling for anything this
// project schedules on its own. Returning false lets the default handler
// still run afterward so millis()/micros()/delay() keep working too (cheap
// to leave alone, and some library init code still uses delay()
// internally), but nothing in this project's own logic depends on them
// anymore.
extern "C" int sysTickHook(void) {
  secondsTimer.tick(1);
  return 0;
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

  pwmOutput.begin();
  counter.add_observer(etl::delegate<void(uint8_t)>::create<CounterDisplay, &CounterDisplay::onCounterChanged>(display));
  counter.add_observer(etl::delegate<void(uint8_t)>::create<CounterPwmOutput, &CounterPwmOutput::onCounterChanged>(pwmOutput));
  counter.begin(); // seed initial display/PWM state

  secondsCounter.add_observer(etl::delegate<void(uint8_t)>::create<CounterDisplay, &CounterDisplay::onCounterChanged>(secondsDisplay));
  secondsCounter.begin(); // seed initial display state

  etl::timer::id::type secondsTimerId = secondsTimer.register_timer(
      etl::delegate<void(void)>::create<NotifyTask, &NotifyTask::notify>(secondsTask), 1000U, true);
  secondsTimer.start(secondsTimerId);

  etl::timer::id::type resetSecondsTimerId = secondsTimer.register_timer(
      etl::delegate<void(void)>::create<NotifyTask, &NotifyTask::notify>(resetSecondsTask), 30000U, true);
  secondsTimer.start(resetSecondsTimerId);

  secondsTimer.enable(true);

  scheduler.add_task(encoderTask);
  scheduler.add_task(leftButtonTask);
  scheduler.add_task(middleButtonTask);
  scheduler.add_task(rightButtonTask);
  scheduler.add_task(secondsTask);
  scheduler.add_task(resetSecondsTask);
  scheduler.set_idle_callback(idleCallback);
}

void loop() {
  // Blocks here running the cooperative task loop for as long as the
  // scheduler is running (i.e. forever, unless something calls
  // scheduler.exit_scheduler()).
  scheduler.start();
}
