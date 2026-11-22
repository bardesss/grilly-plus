# Grilly+ DIY unit — hardware (WIP)

Status: **work in progress**, hardware only. Firmware support (a `diy` build variant) gets its own
design later; nothing here changes the Grilleye firmware. Not linked from the README.

Goal: a standalone 8-probe thermometer that behaves like a Grilleye — screen, buzzer, one button,
battery with fuel gauge, "off" = deep sleep — built from off-the-shelf modules on perfboard.

## Decisions so far

| Topic | Choice | Why |
|---|---|---|
| Probe readout | 2× ADS1115 (I²C 0x48 / 0x49), 4 channels each | Cheap common modules, 16-bit, no mux, 2 wires. Fit only one for a 4-probe build; firmware will detect which ones answer. |
| Divider supply | LM4040A 2.5 V shunt reference | Readings don't drift as the battery drains. |
| Divider resistor | 10 kΩ 0.1 % per channel (same value as the Grilleye) | Good resolution up to ~300 °C; meat temperatures land mid-range. Designs with a 10-bit ADC (HeaterMeter) move to 22k/47k because some probes lose resolution around room temperature with 10 k; with a 16-bit ADC that isn't a problem (~80 counts/°C at 25 °C). |
| Probes | Grilleye probes (100 kΩ NTC, β 4250) by default; any NTC via the custom probe type | Grilly+ already supports custom probe types. |
| Display | ST7565 128×64 LCD, SPI, PWM backlight | Same controller family as the Grilleye; readable in sunlight. |
| Board | DFRobot FireBeetle 2 ESP32-E (DFR0654) | Built-in LiPo charger + USB-C; users measure ~13–25 µA deep sleep once the low-power pad is cut (DFRobot doesn't state a figure — measure it). A generic DevKit draws mA in deep sleep. |
| Power | 18650 + MAX17048 fuel gauge; off = deep sleep (like the Grilleye); switched peripheral rail; master slide switch for storage | Existing power button / auto-off / low-battery-cutoff behaviour carries over. |

## Block diagram

```
 18650 ──[master switch]──┬── FireBeetle BAT (PH2.0)  ── onboard charger ◄── USB-C
                          └── MAX17048 CELL+ (sense, always powered)

 FireBeetle 3V3 (always on, also in deep sleep)
   ├── 100k pull-up ── GPIO35 ── [power button] ── GND
   └── AO3401 P-MOSFET (gate: GPIO26, 100k pull-up to 3V3 → off by default)
          └── RAIL (switched 3V3)
                ├── LM4040 bias (270 Ω) → VREF 2.5 V → 8 probe dividers
                ├── ADS1115 #1 / #2 VDD
                ├── MAX17048 module VIN (I²C pull-ups)
                └── LCD VDD

 I²C (GPIO21 SDA / GPIO22 SCL): ADS1115 0x48, ADS1115 0x49, MAX17048 0x36
```

All I²C pull-ups sit on the switched rail, so nothing back-feeds an unpowered chip in deep sleep.

## Pin map (FireBeetle 2 ESP32-E)

| Function | GPIO | Board label | Notes |
|---|---|---|---|
| I²C SDA | 21 | SDA | ADS1115 ×2, MAX17048 |
| I²C SCL | 22 | SCL | |
| LCD SCK | 18 | SCK | |
| LCD SI / MOSI | 23 | MOSI | |
| LCD CS | 19 | MISO | Used as a plain output |
| LCD A0 / DC | 13 | D7 | |
| LCD RST | 14 | D6 | |
| LCD backlight (PWM) | 4 | D12 | Via NPN; same GPIO as the Grilleye |
| Buzzer (PWM tone) | 25 | D2 | Via NPN |
| Peripheral rail enable | 26 | D3 | **Active low** (P-MOSFET gate) |
| Power button / wake | 35 | A3 | ext0 wake, same GPIO as the Grilleye; input-only, needs the external pull-up |
| Spare | 0, 2, 12, 15, 34, 36, 39 | | 0/12/15 are strapping pins, 2 = onboard LED — avoid for new outputs |

Do not use GPIO 5 (onboard WS2812 data line). GPIO 16/17/27/32/33 aren't on the headers (GPIO 27 is
the FireBeetle's onboard user button).

## Circuits

### Probe channel (×8)

```
 VREF 2.5V ──[10k 0.1%]──┬──[1k]──┬──── ADS1115 AINx
                         │         │
                     jack tip    [100nF]
                       (probe)     │
                     jack sleeve   │
                         │         │
 GND ────────────────────┴─────────┴──── GND
```

- Probe is ground-referenced (sleeve = GND), so the probe's metal side is at ground.
- The 1 kΩ series resistor limits current into the ADC input when the exposed jack or probe picks
  up ESD or touches something it shouldn't; the 100 nF sits on the ADC side of it. Extra error from
  the ADC's input current through 1 kΩ is ~0.02 %.
- Empty jack: the input sits at the full 2.5 V → "disconnected", the same logic as today.
- Expected voltages with a Grilleye probe: 0 °C ≈ 2.43 V, 25 °C ≈ 2.27 V, 55 °C ≈ 1.83 V,
  250 °C ≈ 53 mV (≈ 6–7 ADC counts per °C at PGA ±4.096 V, so still fine).
- Use PGA ±4.096 V (125 µV/LSB); ±2.048 V would clip everything below ~42 °C.
- 2.5 mm **stereo (TRS)** panel jacks, tip + sleeve wired, ring unconnected: they accept both TS and TRS
  plugs. *Verify on a Grilleye probe with a multimeter which contacts carry the NTC.*
- Probes must be **isolated**: the NTC may not connect to the probe's metal body. HeaterMeter warns
  that non-isolated probes can read 50 °C or more off. Check with a multimeter that there's no
  continuity between either plug contact and the probe body.

### Reference

```
 RAIL 3V3 ──[270 Ω]──┬── VREF (2.5 V) ── to all 8 dividers
                     │
               LM4040AIZ-2.5/NOPB (cathode on VREF)   + 10µF + 100nF to GND
                     │
                    GND
```

- Worst-case load (all 8 probes at 250 °C) ≈ 2 mA; 270 Ω keeps the LM4040 in regulation down to a
  3.2 V rail. Idle, the LM4040 sinks ~3 mA — that's why it sits on the switched rail.
- A grade (0.1 %) ≈ 0.25 °C error at room temperature; C grade (0.5 %) ≈ 1.2 °C (fixable with the
  per-probe calibration offset, but A grade is worth the extra euro).
- The ADS1115's own gain error (0.01 % typical, up to 0.15 %) adds to that: worst case about 0.5 °C at
  room temperature with an A-grade reference, also covered by the calibration offset.

### Peripheral rail switch

- AO3401A P-MOSFET (SOT-23 on a DIP adapter): source = 3V3, drain = RAIL, gate = GPIO26 with 100 kΩ to 3V3.
- GPIO26 low = rail on. At boot and in deep sleep (pin held high) the rail is off.
- Logic-level at 3.3 V gate drive; no extra transistor needed.

### LCD backlight and buzzer

- Both via an NPN (BC337 / S8050) low-side switch, 1 kΩ base resistor from the GPIO.
- Backlight: check whether the LCD module has its own LED series resistor; if not, add one
  (target ~20 mA).
- Buzzer: a **passive** buzzer / magnetic transducer (the firmware plays tones, not on/off), 3 V type,
  with a 1N4148 flyback diode across it. Power it from 3V3.

### Power button

- Momentary switch between GPIO35 and GND, 100 kΩ pull-up to 3V3 (always-on), optional 100 nF across
  the switch for debounce.

### Battery and charging

- One protected 18650 in a holder → master slide switch → FireBeetle PH2.0 battery connector.
  Protected cells are ~70 mm long, so pick a holder that fits them.
- MAX17048 CELL sense on the switched-side battery line, so it tracks the cell whenever the unit
  isn't in storage. On the Adafruit module the chip itself runs from the battery; VIN only sets the
  I²C logic level (and powers the pull-ups). With the master switch open it won't answer on I²C.
- Charging through the FireBeetle's USB-C. The charging current is fixed by the board; the DFRobot
  wiki doesn't state it (check the board schematic). A 3000 mAh cell will take several hours.
- Rough runtime: 12–20 h with WiFi and backlight on. Deep sleep: ~30 µA total → years, *if* the
  FireBeetle reaches the user-measured figures below.

### FireBeetle prep

- **Cut the low-power pad** (the thin trace marked on the board). DFRobot only says this saves
  ~500 µA; users measure ~13–25 µA in deep sleep afterwards. **Measure it** on your board before
  relying on the standby figures above. After the cut the onboard RGB LED only works on USB power.

## Bill of materials (rough prices, EUR)

| Qty | Part | ≈ € | Notes |
|---|---|---|---|
| 1 | DFRobot FireBeetle 2 ESP32-E (DFR0654, or DFR0654-F with headers) | 9 | Not the older FireBeetle ESP32 (DFR0478). Active at DFRobot, but reseller stock is thin; consider a spare |
| 2 | ADS1115 module | 2× 3 | **Make sure it's really an ADS1115** (16-bit), not a relabelled ADS1015 (12-bit) |
| 1 | MAX17048 fuel gauge module (e.g. Adafruit 5580 or equivalent) | 6 | Needs separate VIN (I²C) and cell connections |
| 1 | ST7565 128×64 LCD module, **3.3 V logic**, SPI | 8 | E.g. JLX12864G series. Some look-alikes are ST7567/UC1701: they work with U8g2 too, but need a different driver setting |
| 1 | LM4040AIZ-2.5/NOPB (TI, TO-92, 0.1 %) | 1.5 | The old National part without /NOPB is obsolete |
| 8 | 10 kΩ 0.1 % 25 ppm resistor | 8× 0.5 | Divider resistors |
| 8 | 1 kΩ resistor | | ADC input series resistors |
| 1 | 270 Ω resistor | | Reference bias |
| 2 | 100 kΩ resistor | | Rail gate pull-up, button pull-up |
| 2 | 1 kΩ resistor | | NPN base resistors |
| 2 | BC337 or S8050 NPN | | Backlight, buzzer |
| 1 | AO3401A P-MOSFET + SOT-23 → DIP adapter | 1 | Peripheral rail |
| 1 | 1N4148 | | Buzzer flyback |
| ~12 | 100 nF ceramic | | 8× channel filter, reference, decoupling |
| 1 | 10 µF | | Reference |
| 1 | Passive buzzer / magnetic transducer, 3 V (e.g. Same Sky, formerly CUI, CMT-0904-85T: 90 mA, 2.73 kHz) | 2 | Must be passive (tone-driven) |
| 8 | 2.5 mm stereo panel-mount jack | 8× 1 | Fit only 4 for a 4-probe build |
| 1 | Panel-mount momentary button (preferably IP65+) | 3 | |
| 1 | Slide switch | 1 | Master switch for storage |
| 1 | 18650 cell, protected, ~3000 mAh + holder | 8 | |
| 1 | Enclosure (ABS, ~120×80×40 mm) + LCD window | 8 | |
| — | Perfboard, wire, JST PH2.0 lead, headers | 5 | |

Total around **€90–100** for 8 channels, less for 4. Prices are rough (last checked 2026-11 for the
FireBeetle only).

## Practical notes

- Keep the unit away from the lid/firebox: LCD and Li-ion cells don't like > 60 °C.
- Keep the probe wiring on the perfboard short and away from the LCD backlight and buzzer lines.
- Solder the ADS1115 ADDR pin: first module to GND (0x48), second to VDD (0x49). Check the module's
  default (some have ADDR pulled to GND on-board).
- Before putting it in a box: check VREF (2.50 V), rail on/off, and each channel's voltage with a
  known resistor (100 kΩ ≈ 2.27 V).

## Open questions

- The exact ST7565 module (and which U8g2 driver matches it) — decide once a module is picked.
- Grilleye probe jack contacts (TS vs TRS) and isolation from the probe body — measure.
- The ADS1115 input impedance (a few MΩ) against a 10 kΩ source gives ~0.1–0.2 % error; this can be
  corrected in firmware or with the calibration offset. Check on real hardware.
- The FireBeetle charging current and whether it allows charging while the unit is on (load
  sharing).
- Enclosure and panel layout.
- Firmware: separate design (build variant, hardware layer, ADS1115/MAX17048 drivers, pin map).

## Other DIY thermometers (reference)

- [HeaterMeter](https://github.com/CapnBry/HeaterMeter/wiki/HeaterMeter-Probes): 10 k divider
  resistors, mono 2.5 mm jacks; source of the probe isolation warning.
- [BBQMaster](https://github.com/hansaya/BBQMaster): ADS1115 for NTC probes; the author added a battery
  switch afterwards because there was no way to turn it off.
- [bbq-board](https://sr.ht/~martijnbraam/bbq-board/): ESP32-S3 + ADS1115, 100 k probes on 2.5 mm jacks,
  4th channel measures the battery.
- [ESP-BBQ](https://github.com/KMelsen/ESP-BBQ): 100 k divider resistors with 100 nF, balanced around
  room temperature rather than grill temperatures.
- [WLANThermo Nano V3](https://github.com/WLANThermo-nano/WLANThermo_Hardware/tree/master/nano%20v3):
  the most mature open design, with schematic and BOM (non-commercial licence; reference only).
