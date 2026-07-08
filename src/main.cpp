#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <Wire.h>
#include <SparkFun_Qwiic_Twist_Arduino_Library.h>
#include <etl/callback_timer_interrupt.h>
#include <etl/delegate.h>
#include <etl/function.h>
#include <etl/scheduler.h>

#include "adc/AdcInput.h"
#include "counter/CounterPwmOutput.h"
#include "tasks/EncoderTask.h"
#include "tasks/NotifyTask.h"
#include "temperature/SetpointDisplayTask.h"
#include "temperature/TemperatureController.h"
#include "temperature/TemperatureDisplay.h"
#include "temperature/TemperatureSetpoint.h"
#include "temperature/TipTemperature.h"
#include "temperature/TipTemperatureDisplayTask.h"

LGFX tft; // LGFX_AUTODETECT (platformio.ini build_flags) configures this for the WIO Terminal's exact ILI9341/SERCOM7/backlight setup automatically
TWIST twist;

// This panel is 320x240 in landscape (setRotation(1)). Each row's visual
// bounding box is 54px tall - not the raw 48px font 7 digit height, since
// the degree-C suffix's 'C' is bottom-aligned with the digits (see
// TemperatureDisplay::begin()) and sticks up 6px above the digit top.
// Centering two such rows with equal top margin/gap/bottom margin across
// the 240px height solves 3*M + 2*54 = 240, giving M = 44: margin 44, row
// top at y=50 (44+6, since the row's own visual top is y-6), row bottom at
// 98, another 44px gap, second row top at y=148, bottom at 196, then a
// final 44px margin to the bottom edge.
TemperatureDisplay setpointDisplay(tft, 50, TFT_GREEN); // button/encoder controlled
CounterPwmOutput pwmOutput(BCM23);

// The only place the setpoint's value, its 50..450 clamp, and the station's
// on/off state live. Neither producer below knows this type exists beyond
// what's bound into their delegates. Not driven by PWM directly (contrast
// with the old Counter this replaces) - temperatureController polls it
// once per ADC measurement cycle instead (see measureAndResume below);
// setpointDisplayTask polls it for the redraw (see below).
TemperatureSetpoint setpoint;

// onDiff/onPress bind directly to TemperatureSetpoint's own methods - no
// wrapper needed since the delegate signatures already match exactly.
// EncoderTask itself never sees the TemperatureSetpoint type.
EncoderTask encoderTask(twist, BCM21,
                        etl::delegate<void(int16_t)>::create<TemperatureSetpoint, &TemperatureSetpoint::diffSteps>(setpoint),
                        etl::delegate<void(void)>::create<TemperatureSetpoint, &TemperatureSetpoint::toggle>(setpoint));

// Each button needs a fixed argument (+-1 step) or no argument at all baked
// in, which a plain bound-member delegate can't express, hence these small
// lambdas rather than binding straight to TemperatureSetpoint::diffSteps/
// toggle. Like all etl::delegate use in this project, they must be named
// objects with program lifetime, never inline lambda literals (see
// NotifyTask for why).
auto decrementSetpoint = []() { setpoint.diffSteps(-1); };
auto toggleSetpoint = []() { setpoint.toggle(); };
auto incrementSetpoint = []() { setpoint.diffSteps(1); };
NotifyTask leftButtonTask(decrementSetpoint);
NotifyTask middleButtonTask(toggleSetpoint);
NotifyTask rightButtonTask(incrementSetpoint);

// Polls setpoint rather than observing it - see SetpointDisplayTask and
// TemperatureSetpoint's own comments for why the redraw is decoupled from
// whichever task (a button or the encoder) just changed the value.
SetpointDisplayTask setpointDisplayTask(setpoint, setpointDisplay);

// Raw 12-bit ADC reading (0..4095, VDDANA/3.3V reference - both are this
// board's defaults) on A3/BCM24/40-pin header pin 18, sampled once per
// 100ms cycle (see pauseForMeasurement/measureAndResume below - PWM is
// paused around each sample to keep its switching noise out of the
// reading), converted by tipTemperature into a whole-degree Celsius tip
// temperature (see temperature/TipTemperature.h for the sensor/amplifier/
// ADC math). Not built on a Counter/Setpoint-style class - neither a raw
// ADC sample nor a temperature reading has diff/toggle/clamp semantics, so
// that abstraction doesn't fit here.
//
// temperatureDisplayTask (declared after swTimer below, alongside the
// other tasks) polls tipTemperature and redraws on its own turn in the
// scheduler - deliberately decoupled from onAdcSample() itself, since
// that redraw is a slow (~17ms, see project notes) SPI write and
// measureAndResume() needs pwmOutput.resume() to run right after
// sampling, not after whatever a redraw happens to cost that cycle.
TemperatureDisplay temperatureDisplay(tft, 148, TFT_MAGENTA);
TipTemperature tipTemperature;
AdcInput adcInput(A3);

// Turns (setpoint, tipTemperature) into a PWM duty each measurement cycle
// (see measureAndResume below) via br3ttb/Arduino-PID-Library - see
// TemperatureController.h for the on/off handling and why compute()'s
// 100ms cadence matters. Kp/Ki/Kd are placeholders: this project has no
// characterized plant model (heater wattage, thermal mass, sensor lag), so
// these need on-device tuning. Start with pure P (Ki=Kd=0), observe the
// steady-state behaviour, then add Ki to remove any steady-state error, and
// only add Kd afterward if overshoot still needs taming - standard manual
// PID tuning order, since there's no autotune wired up here.
TemperatureController temperatureController(setpoint, /*Kp*/ 5.0, /*Ki*/ 0.0, /*Kd*/ 0.0);

// Guards register_timer()/start()/stop() (called from setup(), i.e. normal
// code) against a concurrent SysTick interrupt calling tick() mid-update -
// tick() itself doesn't need guarding since it *is* that interrupt.
struct SysTickGuard {
  SysTickGuard() { NVIC_DisableIRQ(SysTick_IRQn); }
  ~SysTickGuard() { NVIC_EnableIRQ(SysTick_IRQn); }
};

// A 100ms repeating PWM-pause and a 5ms one-shot ADC measurement (see
// pauseForMeasurement/measureAndResume below) - this is just where this
// project's software timers live. tick() is called with a 1ms count from
// sysTickHook() below, so periods registered on this are in milliseconds.
etl::callback_timer_interrupt<2, SysTickGuard> swTimer;

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
  uint8_t duty = temperatureController.compute(tipTemperature.value());
  // Still paused here - onCounterChanged() only updates lastValue without
  // writing to the pin (see CounterPwmOutput::pause()), so the new duty
  // takes effect exactly when resume() un-pauses it below.
  pwmOutput.onCounterChanged(duty);
  pwmOutput.resume();
};
NotifyTask pwmPauseTask(pauseForMeasurement);
NotifyTask adcSampleTask(measureAndResume);
TipTemperatureDisplayTask temperatureDisplayTask(tipTemperature, temperatureDisplay);

// "Single" (not "multiple"): processes at most one unit of work per task
// per round before moving to the next, so a task whose task_request_work()
// stays true for an extended period (e.g. EncoderTask while the Twist's
// button is held) can't starve the tasks after it in the list. setpoint,
// temperatureController and pwmOutput aren't scheduler tasks at all - each
// update happens synchronously, inline, as part of the producing task's
// task_process_work() call.
etl::scheduler<etl::scheduler_policy_sequential_single, 8> scheduler;

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

  // Text datum is set explicitly by TemperatureDisplay itself on every draw
  // (TR_DATUM for the number, TL_DATUM for the degree-C suffix), not relied
  // on as ambient state here.
  tft.setFont(&fonts::Font7); // 0-9, ':', '-', '.'
  tft.setTextSize(1);
  tft.setTextPadding(TemperatureDisplay::NUMBER_PADDING); // shared with TemperatureDisplay's suffix positioning - see its header

  pwmOutput.begin();
  pwmOutput.onCounterChanged(0); // defined-off PWM state from boot, matching setpoint/temperatureController's initial off state
  setpointDisplay.begin(); // draws the static degree-C suffix once

  analogReadResolution(12); // 0..4095 instead of the default 10-bit 0..1023
  temperatureDisplay.begin(); // draws the static degree-C suffix once
  adcInput.add_observer(etl::delegate<void(uint16_t)>::create<TipTemperature, &TipTemperature::onAdcSample>(tipTemperature));
  adcInput.sample(); // seeds tipTemperature; temperatureDisplayTask's first poll draws the initial number

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
  scheduler.add_task(setpointDisplayTask);
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
