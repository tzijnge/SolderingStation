# Soldering station TODO

## Safety (do first)

* Hard over-temperature cutoff, independent of the PID output
* Thermal runaway detection: high duty for N seconds without temperature rising -> heater off + error on display
* Sensor fault check: tip temperature reading out of plausible range (open/shorted) -> heater off + error on display
* Hardware watchdog (SAMD51 WDT) so a hung loop can never leave the heater on. Heater must be off at power-up until everything is initialized
* Tip detection when inactive: every second inject a small current (short test pulse) and check the measured current (ACS712). No current -> show "No tip"
* Supply voltage check: refuse to heat (and show warning) when the heater supply voltage is outside the expected range (e.g. 19V adapter plugged in by mistake)

## Features

* Set temp should be stored in non-volatile memory, 5 seconds after reaching a stable value.
  * SAMD51 has no EEPROM: use internal flash (FlashStorage-style) or the Wio's QSPI flash
  * Only write when the value differs from what is stored (flash wear). Store temp step in the same record
* Left button should cycle between 1, 5 and 10 degree steps. Selected value should be stored
* Middle button should cycle backlight brightness
* Right button enters shows menu with:
  * help + version info + other info
  * Toggle celcius and fahrenheit
  * ...
* Menu should be implemented with ETL state machine. Qwiic Twist behavior is different in menu that in main screen. Rotate cycles through menu items/values, press selects a value. Separate Qwiic Twist color when in menu
* Joystick
  * left decreases temp
  * right increases temp
  * up selects next larger temp step
  * down selects next smaller temp step
  * press is equal to Qwiic Twist press
* Auto shutdown based on PWM activity (no holder yet). Heater load (PID output jump when the tip touches a joint) or user input counts as activity
  * Consider two stages: drop to standby temperature (e.g. 150-200 degrees) after a few minutes, turn off after a longer idle period. Extends tip life
  * Auto-dim backlight and Qwiic Twist LED in standby
* Qwiic Twist light colors:
  * heating up (far below setpoint)
  * cooling down (far above setpoint)
  * green when close to setpoint
  * blink white when inactive
* Short beep when the tip first reaches the setpoint
* Show heater power (duty %) as a bar on the display
* Small live temperature graph (last ~30 s) on the display
* Add ACS712 module to read average current. Read with spare ADC pin. Read heater supply voltage with spare ADC pin. Limit the power supplied to the iron in a 10 second window. Show warning on display when the power to the iron is capped. Connect ACS712 in reverse so the 'negative' current is measured. This way the ACS712 output will always be below 2.5V and can be connected directly to WIO Terminal ADC input. Concern: The 5 Ampere version is under dimensioned. The 20A version will give quire small analog output most of the time
* Build option to completely disable telemetry
