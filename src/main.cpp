#include <Arduino.h>
#undef round // Arduino.h's round() macro breaks ETL's to_string float formatting
#include <Wire.h>
#include <TFT_eSPI.h>
#include <SparkFun_Qwiic_Twist_Arduino_Library.h>
#include <etl/atomic.h>
#include <etl/delegate.h>
#include <etl/function.h>
#include <etl/scheduler.h>
#include <etl/string.h>
#include <etl/task.h>
#include <etl/to_string.h>

TFT_eSPI tft;
TWIST twist;

// Seeed_GFX/TFT_eSPI has no enum for its built-in fonts, just numeric IDs
// (1, 2, 4, 6, 7, 8) documented in the library README.
constexpr uint8_t SEVEN_SEGMENT_FONT = 7;

constexpr etl::task_priority_t TASK_PRIORITY_INPUT = 2;
constexpr etl::task_priority_t TASK_PRIORITY_DISPLAY = 1;

int32_t counter = 0;

// Interrupt-driven: the Twist's INT pad is wired to BCM21 (verified
// working on-device; BCM2 was tried first but shared a net with the
// Grove I2C bus despite looking independent in the Arduino pin table -
// see project notes). Confirmed against SparkFun's own
// Example8_Interrupts.ino: the line is open-drain, active low
// (INPUT_PULLUP + FALLING), and getCount() alone does NOT clear it -
// isMoved()/isPressed()/isClicked() do, each by clearing its own status
// bit in the Twist's STATUS register (see task_process_work() for why we
// use those instead of the library's blanket clearInterrupts()). The ISR
// can't safely do I2C, so it doesn't do the clearing itself.
//
// Two things had to be set in setup() for the interrupt to fire at all
// and to feel responsive: TWIST_ENABLE_INTS (the library never writes
// it) and setIntTimeout(0) (default firmware batches rotation into one
// interrupt ~250ms after you stop turning).
//
// task_request_work() reads the INT pin's level directly rather than
// counting edges from an ISR flag (contrast with ButtonTask). Reason:
// holding the Twist's own pushbutton down can make its firmware keep
// re-asserting "pressed", regenerating falling edges - with an ISR-set
// edge flag, that meant the flag got set again while it was stuck low
// (no further edges to re-trigger it), so EncoderTask silently stopped
// reporting work forever once that happened (buttons/display kept
// working fine, only the encoder went permanently unresponsive - not a
// scheduler-wide freeze, that was an earlier wrong guess). A level check
// is self-correcting: it naturally returns 0 the instant the line is
// actually released, so it keeps retrying for as long as needed instead
// of giving up after one edge. This also matches how SparkFun's own
// Example8_Interrupts.ino reads this pin - by polling
// digitalRead(pin) == LOW, not via attachInterrupt(). attachInterrupt()
// is still wired up (see onEncoderInterrupt), purely so __WFI() wakes
// promptly on this pin instead of waiting for the next 1ms SysTick tick;
// its callback doesn't need to do anything itself.
class EncoderTask : public etl::task {
public:
  EncoderTask(TWIST &twist_, int32_t &counter_, uint32_t intPin_)
      : etl::task(TASK_PRIORITY_INPUT), twist(twist_), counter(counter_),
        intPin(intPin_) {}

  void on_task_added() override { lastRawCount = twist.getCount(); }

  uint32_t task_request_work() const override {
    return (digitalRead(intPin) == LOW) ? 1u : 0u;
  }

  void task_process_work() override {
    // isMoved()/isPressed()/isClicked() each do their own I2C read of the
    // status byte, then their own I2C write of "that stale snapshot with
    // one bit cleared" - a structural race in the library itself, not
    // just the clearInterrupts() call removed earlier: if a new event
    // arrives on the device between one of these reads and its matching
    // write, that write overwrites the whole byte with the stale
    // snapshot, silently erasing the new event. isClicked() (latched once
    // on button release) only gets one narrow window to be caught, so it
    // occasionally lost the race. isPressed() is checked instead - reset
    // now happens on press (matching the WIO Terminal's own middle
    // button) and, since task_process_work() runs repeatedly for as long
    // as the INT line stays low during a hold, we get many chances to
    // observe it true rather than depending on one race-prone latch.
    twist.isMoved();
    int16_t rawCount = twist.getCount();
    int16_t delta = rawCount - lastRawCount;
    // This board reports clockwise rotation as positive; negate here if
    // your unit behaves the other way.
    counter += delta;
    lastRawCount = rawCount;

    if (twist.isPressed()) {
      counter = 0;
    }

    twist.isClicked(); // unused, but must still be cleared or INT line stays low
  }

private:
  TWIST &twist;
  int32_t &counter;
  uint32_t intPin;
  int16_t lastRawCount = 0;
};

// One button: an ISR (notifyPressed()) increments an atomic pending count,
// and the scheduler drains it, invoking the bound action once per press.
//
// etl::delegate is a non-owning reference to its callable - it just stores
// a pointer to it - so onPress_ must be a named object with program
// lifetime (e.g. a file-scope lambda), never a lambda literal passed
// directly here. A bare temporary compiles fine (even when captureless)
// but leaves onPress pointing at a destroyed object: verified by hand
// against ETL's actual delegate_cpp11.h overload set, since the deleted
// rvalue-lambda constructor only guards lambdas that *aren't* convertible
// to a function pointer, not this case.
class ButtonTask : public etl::task {
public:
  explicit ButtonTask(etl::delegate<void(void)> onPress_)
      : etl::task(TASK_PRIORITY_INPUT), onPress(onPress_) {}

  void notifyPressed() { pending.fetch_add(1); }

  uint32_t task_request_work() const override { return pending.load(); }

  void task_process_work() override {
    pending.fetch_sub(1);
    onPress();
  }

private:
  etl::delegate<void(void)> onPress;
  etl::atomic<uint32_t> pending{0};
};

// Redraws the counter on the TFT whenever it has changed since the last draw.
class DisplayTask : public etl::task {
public:
  DisplayTask(TFT_eSPI &tft_, const int32_t &counter_)
      : etl::task(TASK_PRIORITY_DISPLAY), tft(tft_), counter(counter_) {}

  uint32_t task_request_work() const override {
    return (counter != lastDrawnCounter) ? 1u : 0u;
  }

  void task_process_work() override {
    etl::string<16> text;
    etl::to_string(counter, text);

    // drawString() only clears the padded box below, not the whole screen,
    // so the digits don't flash blank on every update.
    tft.drawString(text.c_str(), 20, 90);
    lastDrawnCounter = counter;
  }

private:
  TFT_eSPI &tft;
  const int32_t &counter;
  int32_t lastDrawnCounter = 0x7FFFFFFF; // force first draw
};

EncoderTask encoderTask(twist, counter, BCM21);
DisplayTask displayTask(tft, counter);

// counter is global, so these don't need to capture it - keeping them
// captureless also means they'd be convertible to a function pointer, but
// they're still passed by name below, never as literals (see ButtonTask).
auto decrementCounter = []() { --counter; };
auto resetCounter = []() { counter = 0; };
auto incrementCounter = []() { ++counter; };

ButtonTask leftButtonTask(decrementCounter);
ButtonTask middleButtonTask(resetCounter);
ButtonTask rightButtonTask(incrementCounter);

// "Single" (not "multiple"): processes at most one unit of work per task
// per round before moving to the next, so a task whose task_request_work()
// stays true for an extended period (e.g. EncoderTask while the Twist's
// button is held) can't starve the tasks after it in the list, like
// DisplayTask, from ever getting a turn.
etl::scheduler<etl::scheduler_policy_sequential_single, 5> scheduler;

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
  scheduler.add_task(displayTask);
  scheduler.set_idle_callback(idleCallback);
}

void loop() {
  // Blocks here running the cooperative task loop for as long as the
  // scheduler is running (i.e. forever, unless something calls
  // scheduler.exit_scheduler()).
  scheduler.start();
}
