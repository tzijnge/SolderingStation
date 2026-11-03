# Regression tests

Manual tests on the device for behaviour that needs a person: something to see, hear or physically do. Run the relevant sections after a change that touches them, and run all of them before a release.

Each test has an ID for reference (e.g. in a PR or an issue), the steps, and the expected result. Tests marked **(code change)** need a temporary edit to the firmware: revert it afterwards and check that it isn't committed. The issue in brackets is where the behaviour was added.

Useful for many tests:

- **Teleplot** via `tools/telemetry.py` (see the README's "Telemetry / PID tuning" section) plots `setpoint`, `measured`, `duty` and `fault` live.
- **lrpcc** (see the README's "Remote control (LotusRPC)" section) reads the state (`lrpcc status state`, `fault`, `setpoint`, `tip_temperature`, `duty`) and simulates the knob and buttons. It can't be used while the telemetry script holds the port.
- **40-pin header:** A3, the tip temperature ADC input, is `BCM24` on pin 18. The nearest GND is pin 20. Pin 17 is 3.3 V, although `variant.h` labels it GND.

## 1. Startup

- **ST-1 Normal startup.** Power on with the Qwiic Twist connected.
  Expected: one short beep at the end of startup. The backlight stays on after the beep. The main screen shows the setpoint as `---` (green, station Off) and the live tip temperature (magenta). The heater is off (`duty` 0).
- **ST-2 Qwiic Twist missing.** Power on with the Twist unplugged.
  Expected: "Qwiic Twist not found!" on the screen, and nothing else happens. The heater stays off.

## 2. Input

- **IN-1 Knob turns the station on.** Station Off, turn the knob one detent clockwise.
  Expected: the station turns on, and the setpoint shows the stored setpoint plus 5 °C.
- **IN-2 Knob press toggles On/Off.** Press the knob several times.
  Expected: each press toggles exactly once between On (setpoint shown) and Off (`---`), however long the knob is held.
- **IN-3 Setpoint steps and direction.** Station On, turn clockwise and anticlockwise one detent at a time.
  Expected: clockwise +5 °C, anticlockwise −5 °C, one step per detent.
- **IN-4 Setpoint limits.** Turn far past both ends.
  Expected: the setpoint stops at 50 °C and at 430 °C.
- **IN-5 Fast turning.** Turn the knob quickly over a large range.
  Expected: the display follows without lag or bursts and stops on the final value.
- **IN-6 Press while turning.** Hold the knob pressed and turn it.
  Expected: one On/Off toggle and no setpoint change.
- **IN-7 Encoder after a long hold.** Hold the knob pressed for about 10 s, release, then turn.
  Expected: the encoder still responds.
- **IN-8 Top buttons.** Press the left, middle and right buttons (left = `WIO_KEY_C`, right = `WIO_KEY_A`).
  Expected: left −5 °C, right +5 °C, and while Off either one turns the station on with that step. Middle toggles On/Off like a knob press.

## 3. Display

- **DI-1 Main screen.** Station On, heating up and holding the setpoint.
  Expected: the setpoint (green, top) and tip temperature (magenta, below) with a °C suffix, no flicker, and only changed digits redrawn.
- **DI-2 Off state.** Turn the station Off.
  Expected: the setpoint shows `---`, and the tip temperature keeps updating while the tip cools.

## 4. Temperature measurement

- **TM-1 Accuracy.** Measure the tip with the FG-100 tip thermometer at setpoints of about 200, 300 and 400 °C.
  Expected: the displayed temperature is within ±5 °C of the FG-100 reading. There's no cold-junction compensation, so allow an extra offset of about (ambient − 17 °C).
- **TM-2 Room temperature.** Cold tip, station Off.
  Expected: a plausible room temperature reading that doesn't jump around by more than a few degrees.

## 5. Heater control

- **HC-1 Heat-up.** From cold, set 350 °C and switch on.
  Expected: 350 °C reached in about 5 s, overshoot of a few degrees at most, and a stable reading at the setpoint within a few seconds.
- **HC-2 Holding under load.** At 330–350 °C, solder joints on a large ground plane.
  Expected: the temperature recovers to the setpoint after each joint. `duty` stays well below 200 (measured: peaks around 110).
- **HC-3 Heater off while Off.** Turn the station Off while hot.
  Expected: `duty` drops to 0 at once and the tip cools.

## 6. Safety faults

General fault behaviour, checked as part of every fault test below: the heater turns off immediately (`duty` 0), the fault screen shows the fault's message, the warning sign, the live tip temperature and "Press knob to reset", and the station beeps twice. Turning the knob is ignored. A knob press acknowledges the fault and lands in Off, and heating again takes a second press.

- **SF-1 Over-temperature (#1) (code change).** In `measureAndResume()` (`main.cpp`), temporarily add `(digitalRead(WIO_KEY_A) == LOW ? 500.0f : 0.0f)` to the temperature passed to `safetyMonitor.check()`, so holding the right button fakes a tip above the limit. Station On, hold the right button.
  Expected: "Tip above 450C limit" at once (within one 100 ms cycle).
- **SF-2 Over-temperature stays latched (#1) (code change).** Continue from SF-1.
  1. Keep holding the right button and press the knob. Expected: the acknowledgement is refused, and the fault screen stays.
  2. Release the right button and wait a few seconds. Expected: the fault screen stays (latched, although the condition is gone).
  3. Press the knob. Expected: the main screen in Off.
- **SF-3 Shorted sensor (#2).** Cold tip inserted, setpoint 100 °C, station Off. Connect A3 (pin 18) to GND (pin 20) through about 100 Ω, then switch on. Never do this with a hot tip: the tip heats uncontrolled until the fault trips.
  Expected: "Tip not heating up" about 3 s after switching on.
- **SF-4 No tip (#2).** Remove the tip, set 100 °C and switch on.
  Expected: "Tip not heating up" after about 3 s.
- **SF-5 Heater saturated (#2) (code change).** Temporarily set `SATURATED_CYCLES` in `SafetyMonitor.h` to 20 (2 s). From cold, set 430 °C and switch on.
  Expected: "Heater at full power too long" about 2 s after switching on.
- **SF-6 No false trips (#2).** With the real limits: heat up from cold to 430 °C, and solder and desolder heavy parts (e.g. a heatsink) at 200 °C and at 330–350 °C.
  Expected: no fault.

## 7. Beeps

- **BE-1 Startup beep.** Covered by ST-1: one short beep, and the backlight stays on.
- **BE-2 Fault beep (#2).** During any fault test in section 6.
  Expected: exactly two short beeps when the fault appears, and no more beeps while the fault screen stays up. A second fault after acknowledging beeps twice again.
- **BE-3 No beep stuck on (#2) (code change).** With the PA-1 change in place, also temporarily make the startup beep long: `beeper.beep(20)` at the end of `setup()` (about 4 s). Power on and turn the knob while it's still beeping.
  Expected: the panic screen appears and the buzzer goes silent at once.

## 8. Panic

- **PA-1 Unhandled event (code change).** Temporarily remove `Rotate` from the event list of the `Off` state in `StationFsm.h` (and its `on_event` handler), then turn the knob while Off.
  Expected: the heater stays off, a red "Unhandled FSM event" screen names the event (`Rotate`) and the state (`Off`), the same text appears on serial, and the Twist LED turns red. The device reboots only on a new knob press, not on the press or turn that caused the panic.

## 9. Telemetry

- **TE-1 Teleplot (#24).** Run `python tools/telemetry.py` and open Teleplot.
  Expected: `setpoint` (0 while Off), `measured`, `measuredFiltered`, `measuredAvg`, `duty`, `adcRaw` and `fault` update about 10 times per second. Ctrl+C stops the script with "Stopped".
- **TE-2 Killed telemetry script (#24).** Start `tools/telemetry.py`, kill it without Ctrl+C (close its terminal), then run `lrpcc status state`.
  Expected: `lrpcc` gets the right answer (the station stopped the stream when the port closed).
- **TE-3 No client (#24).** Leave the port closed (or open it in a terminal that doesn't read) and use the station normally for a minute.
  Expected: no stalls; display, knob and heating respond as usual.

## 10. Remote control

- **RC-1 Build (#24).** Delete `.pio` and build.
  Expected: the build creates `.pio/lrpc-venv`, installs LotusRPC and generates the code without manual steps. A second build doesn't regenerate (no "generating" line). Changing `rpc/soldering_station.lrpc.yaml` regenerates on the next build.
- **RC-2 Inputs (#24).** With `lrpcc`: from Off `input rotate 5`; then `input rotate -- -5`, `input press`, `input button left`, `input button middle`, `input button right`, and `input rotate 0`.
  Expected: exactly what the knob and buttons do (IN-1, IN-2, IN-3, IN-8) on the display and in `status state` / `status setpoint`. `rotate 0` does nothing, also from Off.
- **RC-3 Status (#24).** Compare `status state`, `fault`, `setpoint`, `tip_temperature`, `duty` and `all` with the display and Teleplot, Off and On while heating.
  Expected: they match.
- **RC-4 Faults over RPC (#24).** During a fault from section 6: `input rotate 5`, then `input press` while the condition is still present, then after it's gone.
  Expected: as with the knob — rotate is ignored, the press is refused while the condition is present and acknowledges the fault (to Off) once it's gone. `status fault` shows the latched fault until then.
