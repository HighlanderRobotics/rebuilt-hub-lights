// FRC Practice Hub Lights - REBUILT Game
// Implements match timing and light patterns for practice field
// Hardware: Arduino Uno, 2x WS2812B LED strips, start/stop button, auto-winner switch

#include <FastLED.h>

// ==================== PIN DEFINITIONS ====================
#define BUTTON_PIN 2          // Single start/stop button
#define SWITCH_RED 4          // Auto winner: Red alliance
#define SWITCH_BLUE 5         // Auto winner: Blue alliance
#define LED_PIN_RED 6         // Red hub LED strip data
#define LED_PIN_BLUE 7        // Blue hub LED strip data

// ==================== LED CONFIGURATION ====================
#define NUM_LEDS_RED 60       // Number of LEDs in red hub
#define NUM_LEDS_BLUE 60      // Number of LEDs in blue hub

// ==================== TIMING CONSTANTS (milliseconds) ====================
#define AUTO_DURATION 20000           // Autonomous: 20 seconds
#define AUTO_PAUSE_DURATION 3000      // Auto pause: 3 seconds
#define TRANSITION_DURATION 10000     // Transition shift: 10 seconds
#define SHIFT_DURATION 25000          // Each shift: 25 seconds
#define ENDGAME_DURATION 30000        // Endgame: 30 seconds
#define DEACTIVATION_WARNING 3000     // Warning starts 3s before deactivation

// ==================== COLOR DEFINITIONS ====================
#define RED_COLOR CRGB(255, 0, 0)
#define BLUE_COLOR CRGB(0, 0, 255)
#define GREEN_COLOR CRGB(0, 255, 0)
#define PURPLE_COLOR CRGB(50, 0, 50)  // Dim purple for IDLE

// ==================== ANIMATION TUNING ====================
#define PULSE_SPEED 150.0     // Lower = faster pulse
#define CHASE_SPEED 30        // Lower = faster chase
#define CHASE_WIDTH 5         // Number of white LEDs in chase

// ==================== STATE ENUMERATION ====================
enum MatchState {
  IDLE,           // Waiting for match start
  AUTO,           // Autonomous period
  AUTO_PAUSE,     // Scoring settlement
  TRANSITION,     // Transition shift with chase
  SHIFT_1,        // First teleop shift
  SHIFT_2,        // Second teleop shift
  SHIFT_3,        // Third teleop shift
  SHIFT_4,        // Fourth teleop shift
  ENDGAME,        // Endgame period
  MATCH_OVER      // Match complete
};

// ==================== GLOBAL VARIABLES ====================
MatchState currentState = IDLE;
unsigned long stateStartTime = 0;
bool lastButtonState = HIGH;     // INPUT_PULLUP: unpressed = HIGH
bool redWonAuto = false;          // Auto winner flag
unsigned long lastDebugTime = 0;  // For periodic debug output
bool lastWarningState = false;    // Track warning state changes
unsigned long matchStartTime = 0; // Track when match started (for total time)
int matchNumber = 0;              // Track match count

CRGB redLeds[NUM_LEDS_RED];
CRGB blueLeds[NUM_LEDS_BLUE];

// ==================== HELPER FUNCTIONS ====================

// Returns the duration in milliseconds for the given state
unsigned long getStateDuration(MatchState state) {
  switch(state) {
    case IDLE:        return 0xFFFFFFFF;  // Forever
    case AUTO:        return AUTO_DURATION;
    case AUTO_PAUSE:  return AUTO_PAUSE_DURATION;
    case TRANSITION:  return TRANSITION_DURATION;
    case SHIFT_1:     return SHIFT_DURATION;
    case SHIFT_2:     return SHIFT_DURATION;
    case SHIFT_3:     return SHIFT_DURATION;
    case SHIFT_4:     return SHIFT_DURATION;
    case ENDGAME:     return ENDGAME_DURATION;
    case MATCH_OVER:  return 0xFFFFFFFF;  // Forever
    default:          return 0;
  }
}

// Returns the next state in sequence
MatchState getNextState(MatchState state) {
  switch(state) {
    case IDLE:        return AUTO;
    case AUTO:        return AUTO_PAUSE;
    case AUTO_PAUSE:  return TRANSITION;
    case TRANSITION:  return SHIFT_1;
    case SHIFT_1:     return SHIFT_2;
    case SHIFT_2:     return SHIFT_3;
    case SHIFT_3:     return SHIFT_4;
    case SHIFT_4:     return ENDGAME;
    case ENDGAME:     return MATCH_OVER;
    case MATCH_OVER:  return IDLE;
    default:          return IDLE;
  }
}

// Returns human-readable state name for debugging
const char* stateName(MatchState state) {
  switch(state) {
    case IDLE:        return "IDLE";
    case AUTO:        return "AUTO";
    case AUTO_PAUSE:  return "AUTO_PAUSE";
    case TRANSITION:  return "TRANSITION";
    case SHIFT_1:     return "SHIFT_1";
    case SHIFT_2:     return "SHIFT_2";
    case SHIFT_3:     return "SHIFT_3";
    case SHIFT_4:     return "SHIFT_4";
    case ENDGAME:     return "ENDGAME";
    case MATCH_OVER:  return "MATCH_OVER";
    default:          return "UNKNOWN";
  }
}

// Handles state transitions and initial setup
void enterState(MatchState newState) {
  currentState = newState;
  stateStartTime = millis();
  lastWarningState = false;  // Reset warning tracker

  Serial.println();
  Serial.println("========================================");
  Serial.print("STATE TRANSITION: ");
  Serial.println(stateName(newState));
  Serial.print("System time: ");
  Serial.print(millis() / 1000.0, 1);
  Serial.println(" seconds");
  Serial.print("State duration: ");
  unsigned long duration = getStateDuration(newState);
  if(duration == 0xFFFFFFFF) {
    Serial.println("FOREVER (waiting for button)");
  } else {
    Serial.print(duration / 1000.0, 1);
    Serial.println(" seconds");
  }

  // Read auto-winner switch when entering TRANSITION
  if(newState == TRANSITION) {
    Serial.println("Reading auto-winner switch...");
    bool redPin = digitalRead(SWITCH_RED);
    bool bluePin = digitalRead(SWITCH_BLUE);

    Serial.print("  Red pin: ");
    Serial.print(redPin == LOW ? "PRESSED (LOW)" : "RELEASED (HIGH)");
    Serial.print(" | Blue pin: ");
    Serial.println(bluePin == LOW ? "PRESSED (LOW)" : "RELEASED (HIGH)");

    if(redPin == LOW && bluePin == HIGH) {
      redWonAuto = true;
      Serial.println(">>> AUTO WINNER: RED ALLIANCE <<<");
      Serial.println("Shift pattern: Red inactive → Blue inactive → Red inactive → Blue inactive");
    } else if(bluePin == LOW && redPin == HIGH) {
      redWonAuto = false;
      Serial.println(">>> AUTO WINNER: BLUE ALLIANCE <<<");
      Serial.println("Shift pattern: Blue inactive → Red inactive → Blue inactive → Red inactive");
    } else {
      redWonAuto = true;
      Serial.println(">>> AUTO WINNER: NEUTRAL/CENTER (defaulting to RED) <<<");
      Serial.println("Shift pattern: Red inactive → Blue inactive → Red inactive → Blue inactive");
    }
  }

  // Track match start for timing statistics
  if(newState == AUTO) {
    matchStartTime = millis();
    matchNumber++;
    Serial.print(">>> MATCH #");
    Serial.print(matchNumber);
    Serial.println(" STARTED <<<");
  }

  // Print hub status for shift states
  if(newState >= SHIFT_1 && newState <= SHIFT_4) {
    int shiftNum = newState - SHIFT_1 + 1;  // 1, 2, 3, or 4
    bool redActive;

    if(redWonAuto) {
      redActive = ((shiftNum - 1) % 2 == 1);  // Odd shifts: Red active
    } else {
      redActive = ((shiftNum - 1) % 2 == 0);  // Even shifts: Red active
    }

    Serial.print("SHIFT ");
    Serial.print(shiftNum);
    Serial.print(": Red hub ");
    Serial.print(redActive ? "ACTIVE" : "INACTIVE");
    Serial.print(" | Blue hub ");
    Serial.println(redActive ? "INACTIVE" : "ACTIVE");
    Serial.println("Deactivation warning will pulse in last 3 seconds");
  }

  // Print match completion statistics
  if(newState == MATCH_OVER) {
    unsigned long totalMatchTime = millis() - matchStartTime;
    Serial.println();
    Serial.println("##################################################");
    Serial.println("#           MATCH COMPLETE!                      #");
    Serial.println("##################################################");
    Serial.print("Match #");
    Serial.print(matchNumber);
    Serial.println(" Statistics:");
    Serial.print("  Total match time: ");
    Serial.print(totalMatchTime / 1000.0, 2);
    Serial.print(" seconds (");
    int totalSec = totalMatchTime / 1000;
    Serial.print(totalSec / 60);
    Serial.print(":");
    if(totalSec % 60 < 10) Serial.print("0");
    Serial.print(totalSec % 60);
    Serial.println(")");
    Serial.print("  Expected time: 163 seconds (2:43)");
    int delta = totalMatchTime / 1000 - 163;
    if(delta != 0) {
      Serial.print(" [");
      if(delta > 0) Serial.print("+");
      Serial.print(delta);
      Serial.print("s]");
    }
    Serial.println();
    Serial.print("  Auto winner was: ");
    Serial.println(redWonAuto ? "RED" : "BLUE");
    Serial.println("##################################################");
    Serial.println();
  }

  Serial.println("========================================");
  Serial.println();
}

// ==================== DEBUG FUNCTIONS ====================

// Print periodic status updates during match
void printPeriodicStatus() {
  unsigned long now = millis();

  // Print status every 1 second
  if(now - lastDebugTime >= 1000) {
    lastDebugTime = now;

    unsigned long stateElapsed = now - stateStartTime;
    unsigned long stateDuration = getStateDuration(currentState);

    Serial.print("[");
    Serial.print(now / 1000.0, 1);
    Serial.print("s] ");
    Serial.print(stateName(currentState));
    Serial.print(" - Elapsed: ");
    Serial.print(stateElapsed / 1000.0, 1);
    Serial.print("s");

    if(stateDuration != 0xFFFFFFFF) {
      unsigned long remaining = stateDuration - stateElapsed;
      Serial.print(" | Remaining: ");
      Serial.print(remaining / 1000.0, 1);
      Serial.print("s");

      // Show countdown format for clarity
      int remainingSec = remaining / 1000;
      Serial.print(" (");
      Serial.print(remainingSec);
      Serial.print("s)");
    }

    Serial.println();
  }
}

// ==================== LIGHT ANIMATION FUNCTIONS ====================

// Fill a strip with solid color
void setSolidColor(CRGB* leds, int numLeds, CRGB color) {
  for(int i = 0; i < numLeds; i++) {
    leds[i] = color;
  }
}

// Set dim purple for IDLE state
void setDimPurple(CRGB* leds, int numLeds) {
  setSolidColor(leds, numLeds, PURPLE_COLOR);
}

// Pulsing color with breathing effect (for deactivation warning)
void setPulsingColor(CRGB* leds, int numLeds, CRGB color) {
  // Use sin() for smooth breathing effect
  float pulse = sin(millis() / PULSE_SPEED);
  int brightness = (int)((pulse + 1.0) * 127);  // 0-254 range

  for(int i = 0; i < numLeds; i++) {
    leds[i] = color;
    leds[i].nscale8(brightness);
  }
}

// White chase effect over alliance color (for TRANSITION)
void setWhiteChase(CRGB* leds, int numLeds, CRGB baseColor) {
  // Moving white "spotlight" over alliance color base
  int chasePosition = (millis() / CHASE_SPEED) % numLeds;

  for(int i = 0; i < numLeds; i++) {
    leds[i] = baseColor;  // Base alliance color

    // Add white to LEDs near chase position (with wraparound)
    int distFromChase = abs(i - chasePosition);
    if(distFromChase > numLeds / 2) {
      distFromChase = numLeds - distFromChase;  // Handle wraparound
    }

    if(distFromChase < CHASE_WIDTH) {
      leds[i] = CRGB::White;
    }
  }
}

// Main function to update lights based on current state
void updateLights() {
  unsigned long stateElapsed = millis() - stateStartTime;
  unsigned long stateDuration = getStateDuration(currentState);

  switch(currentState) {
    case IDLE:
      // Both hubs dim purple
      setDimPurple(redLeds, NUM_LEDS_RED);
      setDimPurple(blueLeds, NUM_LEDS_BLUE);
      break;

    case AUTO:
    case AUTO_PAUSE:
      // Both hubs solid alliance color
      setSolidColor(redLeds, NUM_LEDS_RED, RED_COLOR);
      setSolidColor(blueLeds, NUM_LEDS_BLUE, BLUE_COLOR);
      break;

    case TRANSITION:
      // Both hubs active with white chase on hub that goes inactive first
      if(redWonAuto) {
        // Red goes inactive first, so red hub gets chase
        setWhiteChase(redLeds, NUM_LEDS_RED, RED_COLOR);
        setSolidColor(blueLeds, NUM_LEDS_BLUE, BLUE_COLOR);
      } else {
        // Blue goes inactive first, so blue hub gets chase
        setSolidColor(redLeds, NUM_LEDS_RED, RED_COLOR);
        setWhiteChase(blueLeds, NUM_LEDS_BLUE, BLUE_COLOR);
      }
      break;

    case SHIFT_1:
    case SHIFT_2:
    case SHIFT_3:
    case SHIFT_4: {
      // Determine which hub is active based on shift number and auto winner
      int shiftNum = currentState - SHIFT_1;  // 0, 1, 2, or 3
      bool redActive;

      if(redWonAuto) {
        // Red won auto, so red goes inactive first (Shift 1)
        redActive = (shiftNum % 2 == 1);  // Odd shifts: Red active
      } else {
        // Blue won auto, so blue goes inactive first (Shift 1)
        redActive = (shiftNum % 2 == 0);  // Even shifts: Red active
      }

      // Check if we're in deactivation warning period (last 3 seconds)
      bool inWarning = (stateElapsed >= stateDuration - DEACTIVATION_WARNING);

      // Debug output when entering/exiting warning period
      if(inWarning && !lastWarningState) {
        Serial.println("!!! DEACTIVATION WARNING STARTED - Hub pulsing !!!");
        unsigned long remaining = stateDuration - stateElapsed;
        Serial.print("Time until shift change: ");
        Serial.print(remaining / 1000.0, 1);
        Serial.println(" seconds");
        lastWarningState = true;
      }

      if(redActive) {
        // Red active, blue inactive
        if(inWarning) {
          setPulsingColor(redLeds, NUM_LEDS_RED, RED_COLOR);
        } else {
          setSolidColor(redLeds, NUM_LEDS_RED, RED_COLOR);
        }
        setSolidColor(blueLeds, NUM_LEDS_BLUE, CRGB::Black);  // Off
      } else {
        // Blue active, red inactive
        setSolidColor(redLeds, NUM_LEDS_RED, CRGB::Black);  // Off
        if(inWarning) {
          setPulsingColor(blueLeds, NUM_LEDS_BLUE, BLUE_COLOR);
        } else {
          setSolidColor(blueLeds, NUM_LEDS_BLUE, BLUE_COLOR);
        }
      }
      break;
    }

    case ENDGAME:
      // Both hubs solid alliance color
      setSolidColor(redLeds, NUM_LEDS_RED, RED_COLOR);
      setSolidColor(blueLeds, NUM_LEDS_BLUE, BLUE_COLOR);
      break;

    case MATCH_OVER:
      // Both hubs green (field safe)
      setSolidColor(redLeds, NUM_LEDS_RED, GREEN_COLOR);
      setSolidColor(blueLeds, NUM_LEDS_BLUE, GREEN_COLOR);
      break;
  }

  // Push colors to LED strips
  FastLED.show();
}

// ==================== SETUP ====================
void setup() {
  // Initialize Serial for debugging
  Serial.begin(9600);
  delay(1000);  // Wait for Serial to stabilize

  Serial.println();
  Serial.println("=================================================");
  Serial.println("    FRC Practice Hub Lights - REBUILT Game");
  Serial.println("=================================================");
  Serial.println();
  Serial.println("Hardware Configuration:");
  Serial.println("  Arduino Uno with WS2812B LED strips");
  Serial.print("  Red hub LEDs: ");
  Serial.println(NUM_LEDS_RED);
  Serial.print("  Blue hub LEDs: ");
  Serial.println(NUM_LEDS_BLUE);
  Serial.println();

  Serial.println("Pin Configuration:");
  Serial.print("  Start/Stop button: Pin ");
  Serial.println(BUTTON_PIN);
  Serial.print("  Auto winner - Red: Pin ");
  Serial.println(SWITCH_RED);
  Serial.print("  Auto winner - Blue: Pin ");
  Serial.println(SWITCH_BLUE);
  Serial.print("  Red hub LED data: Pin ");
  Serial.println(LED_PIN_RED);
  Serial.print("  Blue hub LED data: Pin ");
  Serial.println(LED_PIN_BLUE);
  Serial.println();

  // Configure input pins
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(SWITCH_RED, INPUT_PULLUP);
  pinMode(SWITCH_BLUE, INPUT_PULLUP);

  Serial.println("Timing Configuration:");
  Serial.print("  AUTO: ");
  Serial.print(AUTO_DURATION / 1000);
  Serial.println(" seconds");
  Serial.print("  AUTO_PAUSE: ");
  Serial.print(AUTO_PAUSE_DURATION / 1000);
  Serial.println(" seconds");
  Serial.print("  TRANSITION: ");
  Serial.print(TRANSITION_DURATION / 1000);
  Serial.println(" seconds");
  Serial.print("  SHIFT (each): ");
  Serial.print(SHIFT_DURATION / 1000);
  Serial.println(" seconds");
  Serial.print("  ENDGAME: ");
  Serial.print(ENDGAME_DURATION / 1000);
  Serial.println(" seconds");
  Serial.print("  Total match time: ");
  int totalSeconds = (AUTO_DURATION + AUTO_PAUSE_DURATION + TRANSITION_DURATION +
                      4 * SHIFT_DURATION + ENDGAME_DURATION) / 1000;
  Serial.print(totalSeconds / 60);
  Serial.print(":");
  if(totalSeconds % 60 < 10) Serial.print("0");
  Serial.print(totalSeconds % 60);
  Serial.println(" (2:43)");
  Serial.println();

  // Initialize LED strips
  Serial.println("Initializing LED strips...");
  FastLED.addLeds<WS2812B, LED_PIN_RED, GRB>(redLeds, NUM_LEDS_RED);
  FastLED.addLeds<WS2812B, LED_PIN_BLUE, GRB>(blueLeds, NUM_LEDS_BLUE);
  FastLED.setBrightness(80);  // Set global brightness (0-255)
  Serial.println("  LED strips initialized (color order: GRB)");
  Serial.println("  Global brightness: 80/255");
  Serial.println();

  // Test initial pin states
  Serial.println("Initial Pin States:");
  Serial.print("  Button: ");
  Serial.println(digitalRead(BUTTON_PIN) == HIGH ? "Released (HIGH)" : "Pressed (LOW)");
  Serial.print("  Red switch: ");
  Serial.println(digitalRead(SWITCH_RED) == HIGH ? "Released (HIGH)" : "Pressed (LOW)");
  Serial.print("  Blue switch: ");
  Serial.println(digitalRead(SWITCH_BLUE) == HIGH ? "Released (HIGH)" : "Pressed (LOW)");
  Serial.println();

  // Enter initial state
  enterState(IDLE);

  Serial.println("=================================================");
  Serial.println("           SYSTEM READY!");
  Serial.println("=================================================");
  Serial.println("Press button to start match");
  Serial.println("Use auto-winner switch before/during TRANSITION");
  Serial.println("Press button during match to emergency stop");
  Serial.println("=================================================");
  Serial.println();
}

// ==================== MAIN LOOP ====================
void loop() {
  // Read button with edge detection
  bool currentButtonState = digitalRead(BUTTON_PIN);

  // Detect button press (HIGH to LOW transition)
  if(lastButtonState == HIGH && currentButtonState == LOW) {
    Serial.println();
    Serial.println("**************************************");
    Serial.println("*** BUTTON PRESS DETECTED ***");
    Serial.print("Current state: ");
    Serial.println(stateName(currentState));
    Serial.print("System time: ");
    Serial.print(millis() / 1000.0, 1);
    Serial.println(" seconds");

    if(currentState == IDLE || currentState == MATCH_OVER) {
      // Start new match
      Serial.println("Action: STARTING NEW MATCH");
      Serial.println("**************************************");
      enterState(AUTO);
    } else {
      // Stop match immediately (return to IDLE)
      Serial.println("Action: EMERGENCY STOP - Returning to IDLE");
      unsigned long matchElapsed = millis() - stateStartTime;
      Serial.print("Match interrupted at: ");
      Serial.print(matchElapsed / 1000.0, 1);
      Serial.println(" seconds into current state");
      Serial.println("**************************************");
      enterState(IDLE);
    }
  }
  lastButtonState = currentButtonState;

  // Check for state timer expiration
  unsigned long stateElapsed = millis() - stateStartTime;
  unsigned long stateDuration = getStateDuration(currentState);

  if(stateElapsed >= stateDuration) {
    // Time to transition to next state
    Serial.println();
    Serial.println("--- Timer expired, transitioning to next state ---");
    MatchState nextState = getNextState(currentState);
    enterState(nextState);
  }

  // Print periodic status updates (except for IDLE and MATCH_OVER)
  if(currentState != IDLE && currentState != MATCH_OVER) {
    printPeriodicStatus();
  }

  // Update LED strips
  updateLights();

  // Small delay to prevent Serial flooding and allow button debouncing
  delay(10);
}
