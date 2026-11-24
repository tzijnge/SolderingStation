# Hardware modifications to the Elektor design

Changes made to the controller board and wiring of the original
[Elektor Platino soldering station](https://www.elektormagazine.com/magazine/elektor-201507/27978), which was
designed for a 5 V microcontroller platform, compared to this build, which uses a 3.3 V WIO Terminal.

| # | Where | Change | Reason |
|---|---|---|---|
| 1 | Controller board, R6 | Changed to **12 Ω** (original value: 1 kΩ) | With the original value the heater MOSFET didn't turn on completely from the WIO Terminal's 3.3 V PWM output (the original design drives it from 5 V) |
| 2 | Between the controller board and the WIO Terminal, ADC input A3 (header pin 18) and GND (pin 20) | Twisted pair, plus a **100 Ω** resistor in series with A3 and **100 nF** from A3 to GND, both at the WIO Terminal's header pins | Brief spikes in single ADC reads (about one per 19 s at 300 °C) from the mains adapter's field, which made the PID burst the heater and the tip overshoot. See below |

## 2. Twisted wire and RC filter on the ADC input

- The resistor is in series with the A3 wire, and the capacitor goes from the A3 side of the resistor to GND, directly
  at the WIO Terminal's header pins. The capacitor must sit at the ADC pin: the resistor and capacitor together make the
  low-pass filter. The wires from the board to the WIO stay twisted.
- The controller board already has a 10 nF capacitor from the amplifier output to ground at the connector. It stays.
- Measured with the real wiring (amplifier and tip connected, mains adapter, station On), as odd cycles per 15 min
  (one ADC read of three off by 20 counts or more from the other two):

  | Setup | 50 °C | 300 °C |
  |---|---|---|
  | No filter | about 23 per 2 min | about 1 per 19 s |
  | Twisted wire only | no measurable difference | |
  | 1 kΩ + 100 nF, twisted | 0 | 0 |
  | 100 Ω + 100 nF, twisted | 0 | |

- 100 Ω is slightly quieter than 1 kΩ (read-to-read noise 2.3 against 3.4 counts) and both removed the glitches.
- The cause is outside the ADC and its supply: with an AA cell on A3 and the amplifier disconnected, the spikes were
  absent on USB power alone and nearly absent on a bench supply, and were present with the mains adapter.
- No firmware change was needed (issue #28, a median of three reads, was closed as superseded).
