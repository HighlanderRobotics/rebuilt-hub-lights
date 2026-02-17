# FRC Practice Hub Lights - REBUILT Game

Arduino-based practice field lighting system that recreates authentic FRC REBUILT (2025 game) match timing and hub light patterns. At a real competition, the Field Management System (FMS) controls hub lights that tell drivers which hub is active, which is about to deactivate, and when shifts happen. Practice fields don't have any of that -- drivers just drive around with no timing cues. This project fixes that gap so drivers can build muscle memory for match phases with realistic visual feedback.

This is a standalone system -- it doesn't connect to FMS, Cheesy Arena, or track score. It just does lights and timing. Someone manually sets which alliance "won auto" via a physical switch, since we don't have scoring sensors.

## How a REBUILT Match Works

A match is 2 minutes 43 seconds total, structured as:

| Phase | Duration | Hub Behavior |
|-------|----------|-------------|
| **AUTO** | 20s | Both hubs solid alliance color (both active) |
| **Pause** | 3s | Brief scoring settle |
| **Transition** | 10s | Both active; white chase pattern on the hub going inactive first |
| **Shift 1** | 25s | One hub active, one off. 3-sec pulsing warning before swap |
| **Shift 2** | 25s | Hubs swap active/inactive |
| **Shift 3** | 25s | Swap again |
| **Shift 4** | 25s | Swap again |
| **Endgame** | 30s | Both hubs active again |

The alliance that scored more in AUTO gets the advantage: the *losing* alliance's hub goes inactive first in Shift 1. Hubs then alternate every 25 seconds. Active hubs pulse their alliance color for 3 seconds before deactivating as a warning.

## State Machine

The code is built as a linear state machine using non-blocking timing (`millis()`, not `delay()`):

```
IDLE -> AUTO -> AUTO_PAUSE -> TRANSITION -> SHIFT_1 -> SHIFT_2 -> SHIFT_3 -> SHIFT_4 -> ENDGAME -> MATCH_OVER
```

The start button triggers IDLE -> AUTO. After that, all transitions are timer-driven. The button acts as an emergency stop from any state back to IDLE.

## Hardware

- **Arduino Uno** (or Mega for 60+ LEDs per hub)
- **2x WS2812B LED strips** (30 LEDs per hub default, configurable)
- **Start button** - green momentary pushbutton (pin 2)
- **Auto-winner 3-position toggle switch** (pins 4/5) - left = red won auto, center = neutral, right = blue won auto
- **DFPlayer Mini MP3/WAV player** with micro SD card
- **Speaker or headphones** for audio output
- **1kΩ resistor** for DFPlayer RX protection
- **2x 5V power supplies** for LED strips (60-100W each, separate from Arduino)
- **2x 330-ohm resistors** on data lines (signal reflection protection)
- **2x 1000uF capacitors** across power lines (voltage spike protection)

## Pin Mapping

| Pin | Function |
|-----|----------|
| 2 | Start/stop button (INPUT_PULLUP) |
| 4 | Auto-winner switch - red position (INPUT_PULLUP) |
| 5 | Auto-winner switch - blue position (INPUT_PULLUP) |
| 6 | Red alliance LED strip data |
| 7 | Blue alliance LED strip data |
| 10 | DFPlayer serial RX (from DFPlayer TX) |
| 11 | DFPlayer serial TX (to DFPlayer RX via 1kΩ resistor) |

## Usage

1. **Install libraries** in Arduino IDE: FastLED and DFPlayerMini_Fast
2. **Prepare SD card** - Format as FAT32, create `MP3` folder, copy audio files from Assets folder as `0001.wav` through `0004.wav`
3. **Insert SD card** into DFPlayer Mini
4. **Upload** `frc_lights_rebuilt.ino` to your Arduino
5. **Wire** LEDs to pins 6 (red hub) and 7 (blue hub), button to pin 2, toggle switch to pins 4/5, DFPlayer to pins 10/11
6. **Press button** to start match -- auto-runs through all phases (2:43 total)
7. **Set auto-winner switch** before or during Transition to control which hub goes inactive first

Press the button during a match to emergency stop (returns to IDLE).

## Light States

- **Solid alliance color** -- hub is active, score here
- **Pulsing alliance color** -- warning, hub deactivating in 3 seconds
- **Alliance color + white chase** -- during Transition, indicates which hub goes inactive first
- **Off** -- hub is inactive
- **Green** -- match over, field safe
- **Dim purple** -- idle, waiting for match start

## Wiring Notes

- Power LED strips from dedicated 5V supplies, **not** from the Arduino
- **All grounds must connect together** (Arduino GND + power supply GND + LED strip GND)
- Use 16 AWG wire for power runs to avoid voltage drop
- Place 330-ohm resistor between Arduino data pin and LED strip data input
- Place 1000uF capacitor across +5V/GND at each strip (striped side to GND)
- Use 1kΩ resistor between Arduino pin 11 and DFPlayer RX pin
- DFPlayer shares 5V and GND with Arduino (powered from Arduino)
- SD card must be FAT32 format with files in `MP3` folder

## Audio Cues

The system plays audio cues at key match moments to help drivers develop auditory recognition of match transitions:

| Sound File | Plays When | Match Phase |
|------------|-----------|-------------|
| 0001.wav (start) | AUTO state begins | Match start (0:00) |
| 0004.wav (end) | AUTO_PAUSE state begins | End of autonomous (0:20) |
| 0003.wav (resume) | TRANSITION state begins | Teleop starts (0:23) |
| 0002.wav (warning) | ENDGAME state begins | Endgame warning (2:13) |
| 0004.wav (end) | MATCH_OVER state begins | Match complete (2:43) |

Audio files are stored on a micro SD card in the DFPlayer Mini. The volume is set to 25 (out of 30) by default and can be adjusted in the code.
