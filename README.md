  # FRC Practice Hub Lights - REBUILT Game

  Arduino-based practice field lighting system that recreates authentic FRC REBUILT match timing and hub light patterns. Helps drivers build muscle memory for match phases with realistic visual cues.

  ## Hardware

  - Arduino Uno (or Mega for 60+ LEDs per hub)
  - 2× WS2812B LED strips (30 LEDs per hub default, configurable)
  - Start/Stop button (pin 2)
  - Auto-winner 3-position switch (pins 4/5)
  - 2× 5V power supplies for LED strips (separate from Arduino)

  ## Usage

  1. **Install FastLED library** in Arduino IDE
  2. **Upload** `frc_lights_rebuilt.ino` to your Arduino
  3. **Wire** LEDs to pins 6 (red) and 7 (blue), button to pin 2, switches to pins 4/5
  4. **Press button** to start match - auto-runs through all states (2:43 total)
  5. **Set auto-winner switch** during TRANSITION to control which hub goes inactive first

  Match runs automatically: AUTO (20s) → PAUSE (3s) → TRANSITION (10s) → 4 SHIFTS (25s each) → ENDGAME (30s). Active hubs pulse in last 3 seconds before deactivating. Press button during match to emergency stop.

  ⚠️ **Power LEDs separately from Arduino - connect all grounds together**
