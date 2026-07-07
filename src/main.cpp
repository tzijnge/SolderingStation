#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <Wire.h>
#include <SparkFun_Qwiic_Twist_Arduino_Library.h>
#include <etl/callback_timer_interrupt.h>
#include <etl/delegate.h>
#include <etl/function.h>
#include <etl/scheduler.h>

#include "adc/AdcInput.h"
#include "counter/Counter.h"
#include "counter/CounterDisplay.h"
#include "counter/CounterDisplayTask.h"
#include "counter/CounterPwmOutput.h"
#include "tasks/EncoderTask.h"
#include "tasks/NotifyTask.h"
#include "temperature/TipTemperature.h"
#include "temperature/TipTemperatureDisplay.h"
#include "temperature/TipTemperatureDisplayTask.h"

LGFX tft; // LGFX_AUTODETECT (platformio.ini build_flags) configures this for the WIO Terminal's exact ILI9341/SERCOM7/backlight setup automatically
TWIST twist;

// This panel is 320x240 in landscape (setRotation(3)), so three 48px-tall
// font 7 rows have to fit within a 240px height - evenly spaced 80px apart
// (20, 100, 180) leaves a comfortable margin above/below/between all three.
CounterDisplay display(tft, 20, TFT_GREEN); // button/encoder controlled
CounterPwmOutput pwmOutput(BCM23);

// The only place the counter's value and its 0..255 clamp live. Neither
// producer below knows this type exists beyond what's bound into their
// delegates. pwmOutput observes it directly (see counter.add_observer()
// in setup()); counterDisplayTask polls it instead (see below).
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

// Polls counter rather than observing it - see CounterDisplayTask and
// Counter's own comments for why the redraw is decoupled from whichever
// task (a button or the encoder) just changed the value.
CounterDisplayTask counterDisplayTask(counter, display);

// A second, independent counter driven by 1-second and 30-second software
// timers instead of hardware input, just to prove the timer plumbing out -
// see swTimer/sysTickHook() below for how they actually tick.
CounterDisplay secondsDisplay(tft, 100, TFT_YELLOW); // timer controlled
Counter secondsCounter;
auto incrementSeconds = []() { secondsCounter.diff(1); };
auto resetSeconds = []() { secondsCounter.reset(); };
NotifyTask secondsTask(incrementSeconds);
NotifyTask resetSecondsTask(resetSeconds);
CounterDisplayTask secondsDisplayTask(secondsCounter, secondsDisplay);

// Raw 12-bit ADC reading (0..4095, VDDANA/3.3V reference - both are this
// board's defaults) on A3/BCM24/40-pin header pin 18, sampled once per
// 100ms cycle (see pauseForMeasurement/measureAndResume below - PWM is
// paused around each sample to keep its switching noise out of the
// reading), converted by tipTemperature into a whole-degree Celsius tip
// temperature (see temperature/TipTemperature.h for the sensor/amplifier/
// ADC math). Not built on Counter - neither a raw ADC sample nor a
// temperature reading has diff/reset/clamp semantics, so that abstraction
// doesn't fit here.
//
// temperatureDisplayTask (declared after swTimer below, alongside the
// other tasks) polls tipTemperature and redraws on its own turn in the
// scheduler - deliberately decoupled from onAdcSample() itself, since
// that redraw is a slow (~17ms, see project notes) SPI write and
// measureAndResume() needs pwmOutput.resume() to run right after
// sampling, not after whatever a redraw happens to cost that cycle.
TipTemperatureDisplay temperatureDisplay(tft, 180, TFT_MAGENTA);
TipTemperature tipTemperature;
AdcInput adcInput(A3);

// Guards register_timer()/start()/stop() (called from setup(), i.e. normal
// code) against a concurrent SysTick interrupt calling tick() mid-update -
// tick() itself doesn't need guarding since it *is* that interrupt.
struct SysTickGuard {
  SysTickGuard() { NVIC_DisableIRQ(SysTick_IRQn); }
  ~SysTickGuard() { NVIC_EnableIRQ(SysTick_IRQn); }
};

// A 1-second repeating counter tick, a 30-second repeating counter reset,
// a 100ms repeating PWM-pause, and a 5ms one-shot ADC measurement (see
// pauseForMeasurement/measureAndResume below) - all unrelated to each
// other, this is just where this project's software timers live. tick()
// is called with a 1ms count from sysTickHook() below, so periods
// registered on this are in milliseconds.
etl::callback_timer_interrupt<4, SysTickGuard> swTimer;

// pwmOutput switching is a noise source for the ADC reading, so each
// measurement cycle pauses it first: a 100ms repeating timer pauses PWM
// and arms a 5ms one-shot (registered once in setup(), non-repeating,
// just re-started here each cycle) to give the switching noise time to
// settle before the actual sample is taken and PWM resumed.
etl::timer::id::type measureTimerId; // assigned once in setup()

auto pauseForMeasurement = []() {
  pwmOutput.pause();
  swTimer.start(measureTimerId);
};
auto measureAndResume = []() {
  adcInput.sample();
  pwmOutput.resume();
};
NotifyTask pwmPauseTask(pauseForMeasurement);
NotifyTask adcSampleTask(measureAndResume);
TipTemperatureDisplayTask temperatureDisplayTask(tipTemperature, temperatureDisplay);

// "Single" (not "multiple"): processes at most one unit of work per task
// per round before moving to the next, so a task whose task_request_work()
// stays true for an extended period (e.g. EncoderTask while the Twist's
// button is held) can't starve the tasks after it in the list. counter,
// display and pwmOutput aren't scheduler tasks at all - each update
// happens synchronously, inline, as part of the producing task's
// task_process_work() call.
etl::scheduler<etl::scheduler_policy_sequential_single, 11> scheduler;

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
// default handler (see hooks.c) - this is what actually drives swTimer,
// replacing millis()/delay()-based polling for anything this project
// schedules on its own. Returning false lets the default handler still run
// afterward so millis()/micros()/delay() keep working too (cheap to leave
// alone, and some library init code still uses delay() internally), but
// nothing in this project's own logic depends on them anymore.
extern "C" int sysTickHook(void) {
  swTimer.tick(1);
  return 0;
}

void setup() {
  Wire.begin();
  Wire.setClock(400000); // fast-mode I2C, keeps polling latency low

  tft.begin();
  // LovyanGFX's rotation numbering doesn't match Seeed_GFX's for this panel -
  // its own WIO Terminal autodetect (LGFX_AutoDetect_SAMD51.hpp) calibrates
  // rotation 1 as this board's correct upright landscape orientation.
  tft.setRotation(1);
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
  tft.setFont(&fonts::Font7); // 0-9, ':', '-', '.'
  tft.setTextSize(1);
  tft.setTextPadding(TipTemperatureDisplay::NUMBER_PADDING); // shared with TipTemperatureDisplay's suffix positioning - see its header

  pwmOutput.begin();
  counter.add_observer(etl::delegate<void(uint8_t)>::create<CounterPwmOutput, &CounterPwmOutput::onCounterChanged>(pwmOutput));
  counter.begin(); // seed initial PWM state (counterDisplayTask seeds the display itself, on its first poll)

  analogReadResolution(12); // 0..4095 instead of the default 10-bit 0..1023
  temperatureDisplay.begin(); // draws the static degree-C suffix once
  adcInput.add_observer(etl::delegate<void(uint16_t)>::create<TipTemperature, &TipTemperature::onAdcSample>(tipTemperature));
  adcInput.sample(); // seeds tipTemperature; temperatureDisplayTask's first poll draws the initial number

  etl::timer::id::type secondsTimerId = swTimer.register_timer(
      etl::delegate<void(void)>::create<NotifyTask, &NotifyTask::notify>(secondsTask), 1000U, true);
  swTimer.start(secondsTimerId);

  etl::timer::id::type resetSecondsTimerId = swTimer.register_timer(
      etl::delegate<void(void)>::create<NotifyTask, &NotifyTask::notify>(resetSecondsTask), 30000U, true);
  swTimer.start(resetSecondsTimerId);

  // Registered but not started - only re-started by pauseForMeasurement,
  // repeating=false so it fires exactly once per arm.
  measureTimerId = swTimer.register_timer(
      etl::delegate<void(void)>::create<NotifyTask, &NotifyTask::notify>(adcSampleTask), 5U, false);

  etl::timer::id::type pwmPauseTimerId = swTimer.register_timer(
      etl::delegate<void(void)>::create<NotifyTask, &NotifyTask::notify>(pwmPauseTask), 100U, true);
  swTimer.start(pwmPauseTimerId);

  swTimer.enable(true);

  scheduler.add_task(encoderTask);
  scheduler.add_task(leftButtonTask);
  scheduler.add_task(middleButtonTask);
  scheduler.add_task(rightButtonTask);
  scheduler.add_task(counterDisplayTask);
  scheduler.add_task(secondsTask);
  scheduler.add_task(resetSecondsTask);
  scheduler.add_task(secondsDisplayTask);
  scheduler.add_task(pwmPauseTask);
  scheduler.add_task(adcSampleTask);
  scheduler.add_task(temperatureDisplayTask);
  scheduler.set_idle_callback(idleCallback);
}

void loop() {
  // Blocks here running the cooperative task loop for as long as the
  // scheduler is running (i.e. forever, unless something calls
  // scheduler.exit_scheduler()).
  scheduler.start();
}
