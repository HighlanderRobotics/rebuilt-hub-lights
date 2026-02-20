  // FRC Practice Hub Lights - REBUILT Game
  // Optimized for Arduino Uno memory constraints
  // Hardware: Arduino Uno, 2x WS2812B LED strips, start/stop button, auto-winner switch

  #include <FastLED.h>
  #include <SoftwareSerial.h>
  #include <DFPlayerMini_Fast.h>
  #include <IRremote.hpp>

  // ==================== PIN DEFINITIONS ====================
  #define BUTTON_PIN 2
  #define SWITCH_RED 4
  #define SWITCH_BLUE 5
  #define LED_PIN_RED 6
  #define LED_PIN_BLUE 7
  #define IR_RECEIVE_PIN 8
  #define IR_SEND_PIN 3
  #define DFPLAYER_RX 10
  #define DFPLAYER_TX 11

  // ==================== LED CONFIGURATION ====================
  #define NUM_LEDS_RED 30       // Optimized for Uno memory
  #define NUM_LEDS_BLUE 30      // Use Arduino Mega for more LEDs

  // ==================== DEBUG CONFIGURATION ====================
  #define AUTOSTART true       // Set to true for auto-start with blue winning auto

  // ==================== TIMING CONSTANTS (milliseconds) ====================
  #define AUTO_DURATION 20000
  #define AUTO_PAUSE_DURATION 5000
  #define TRANSITION_DURATION 10000
  #define SHIFT_DURATION 25000
  #define ENDGAME_DURATION 30000
  #define DEACTIVATION_WARNING 3000

  // ==================== COLOR DEFINITIONS ====================
  #define RED_COLOR CRGB(255, 0, 0)
  #define BLUE_COLOR CRGB(0, 0, 255)
  #define GREEN_COLOR CRGB(0, 255, 0)
  #define PURPLE_COLOR CRGB(50, 0, 50)

  // ==================== ANIMATION TUNING ====================
  #define PULSE_SPEED 50.0
  #define CHASE_SPEED 30
  #define CHASE_WIDTH 5

  // ==================== AUDIO FILE NUMBERS ====================
  // Audio file numbers (matching SD card files)
  #define AUDIO_START 1    // 0001.wav - Match start
  #define AUDIO_WARNING 2  // 0002.wav - Endgame warning
  #define AUDIO_RESUME 3   // 0003.wav - Teleop begins
  #define AUDIO_END 4      // 0004.wav - End of auto/match

  // ==================== IR TIMER CONTROL ====================
  #define IR_PROTOCOL 8    // Protocol for timer communication
  #define IR_ADDRESS 0x22  // Address for timer
  #define IR_CMD_RESET 0x23     // Reset timer
  #define IR_CMD_STOP 0x20      // Stop timer
  #define IR_CMD_START 0x1F     // Start timer
  #define IR_CMD_AUTO_END 0x24  // Auto phase end
  #define IR_CMD_TELEOP_START 0x25  // Teleop phase start

  // ==================== STATE ENUMERATION ====================
  enum MatchState {
    IDLE, AUTO, AUTO_PAUSE, TRANSITION,
    SHIFT_1, SHIFT_2, SHIFT_3, SHIFT_4,
    ENDGAME, MATCH_OVER
  };

  // ==================== GLOBAL VARIABLES ====================
  MatchState currentState = IDLE;
  unsigned long stateStartTime = 0;
  bool lastButtonState = HIGH;
  bool redWonAuto = false;
  unsigned long lastDebugTime = 0;
  bool lastWarningState = false;
  unsigned long matchStartTime = 0;
  int matchNumber = 0;

  CRGB redLeds[NUM_LEDS_RED];
  CRGB blueLeds[NUM_LEDS_BLUE];

  SoftwareSerial dfSerial(DFPLAYER_RX, DFPLAYER_TX);
  DFPlayerMini_Fast myMP3;

  // ==================== HELPER FUNCTIONS ====================

  unsigned long getStateDuration(MatchState state) {
    switch(state) {
      case IDLE: case MATCH_OVER: return 0xFFFFFFFF;
      case AUTO: return AUTO_DURATION;
      case AUTO_PAUSE: return AUTO_PAUSE_DURATION;
      case TRANSITION: return TRANSITION_DURATION;
      case SHIFT_1: case SHIFT_2: case SHIFT_3: case SHIFT_4: return SHIFT_DURATION;
      case ENDGAME: return ENDGAME_DURATION;
      default: return 0;
    }
  }

  MatchState getNextState(MatchState state) {
    switch(state) {
      case IDLE: return AUTO;
      case AUTO: return AUTO_PAUSE;
      case AUTO_PAUSE: return TRANSITION;
      case TRANSITION: return SHIFT_1;
      case SHIFT_1: return SHIFT_2;
      case SHIFT_2: return SHIFT_3;
      case SHIFT_3: return SHIFT_4;
      case SHIFT_4: return ENDGAME;
      case ENDGAME: return MATCH_OVER;
      default: return IDLE;
    }
  }

  // State names stored in Flash memory to save SRAM
  const char str_IDLE[] PROGMEM = "IDLE";
  const char str_AUTO[] PROGMEM = "AUTO";
  const char str_AUTO_PAUSE[] PROGMEM = "AUTOPAUSE";
  const char str_TRANSITION[] PROGMEM = "TRANS";
  const char str_SHIFT_1[] PROGMEM = "SHIFT1";
  const char str_SHIFT_2[] PROGMEM = "SHIFT2";
  const char str_SHIFT_3[] PROGMEM = "SHIFT3";
  const char str_SHIFT_4[] PROGMEM = "SHIFT4";
  const char str_ENDGAME[] PROGMEM = "ENDGAME";
  const char str_MATCH_OVER[] PROGMEM = "DONE";
  const char str_UNKNOWN[] PROGMEM = "???";

  const char* const stateNames[] PROGMEM = {
    str_IDLE, str_AUTO, str_AUTO_PAUSE, str_TRANSITION,
    str_SHIFT_1, str_SHIFT_2, str_SHIFT_3, str_SHIFT_4,
    str_ENDGAME, str_MATCH_OVER
  };

  const char* stateName(MatchState state) {
    if(state >= 0 && state <= MATCH_OVER) {
      return (const char*)pgm_read_word(&(stateNames[state]));
    }
    return (const char*)pgm_read_word(&str_UNKNOWN);
  }

  void sendIRCommand(uint8_t command) {
    IrSender.sendRC5(IR_ADDRESS, command, 0, true);
    delay(100);
  }

  void enterState(MatchState newState) {
    currentState = newState;
    stateStartTime = millis();
    lastWarningState = false;

    // Trigger sounds for state transitions
    bool shouldPlaySound = false;
    int soundFile = 0;

    switch(newState) {
      case AUTO:
        soundFile = AUDIO_START;
        shouldPlaySound = true;
        break;

      case AUTO_PAUSE:
        soundFile = AUDIO_END;
        shouldPlaySound = true;
        break;

      case TRANSITION:
        soundFile = AUDIO_RESUME;
        shouldPlaySound = true;
        break;

      case ENDGAME:
        soundFile = AUDIO_WARNING;
        shouldPlaySound = true;
        break;

      case MATCH_OVER:
        soundFile = AUDIO_END;
        shouldPlaySound = true;
        break;
    }

    if(shouldPlaySound) {
      myMP3.play(soundFile);
    }

    // Send IR commands to timer based on state
    switch(newState) {
      case AUTO:
        sendIRCommand(IR_CMD_START);  // Start timer when auto begins
        break;

      case AUTO_PAUSE:
        sendIRCommand(IR_CMD_AUTO_END);  // Signal auto phase end
        sendIRCommand(IR_CMD_START);     // Continue timer
        break;

      case TRANSITION:
        sendIRCommand(IR_CMD_TELEOP_START);  // Signal teleop phase start
        sendIRCommand(IR_CMD_START);          // Continue timer
        break;

      case MATCH_OVER:
        sendIRCommand(IR_CMD_RESET);  // Reset timer
        sendIRCommand(IR_CMD_STOP);   // Stop timer
        break;
    }

    Serial.println();
    Serial.print(F("STATE: "));
    Serial.print(stateName(newState));
    Serial.print(F(" @ "));
    Serial.print(millis() / 1000);
    Serial.println(F("s"));

    // Read auto-winner switch
    if(newState == TRANSITION) {
      if(AUTOSTART) {
        redWonAuto = false;
      } else {
        bool redPin = digitalRead(SWITCH_RED);
        bool bluePin = digitalRead(SWITCH_BLUE);
        if(redPin == LOW && bluePin == HIGH) {
          redWonAuto = true;
        } else if(bluePin == LOW && redPin == HIGH) {
          redWonAuto = false;
        } else {
          redWonAuto = true;
        }
      }
    }

    // Track match start
    if(newState == AUTO) {
      matchStartTime = millis();
      matchNumber++;
    }

    // Match completion
    if(newState == MATCH_OVER) {
      unsigned long totalTime = millis() - matchStartTime;
      Serial.print(F("DONE! T="));
      Serial.print(totalTime / 1000);
      Serial.print(F("s"));
      int delta = totalTime / 1000 - 163;
      if(delta != 0) {
        Serial.print(F(" ["));
        if(delta > 0) Serial.print(F("+"));
        Serial.print(delta);
        Serial.print(F("]"));
      }
      Serial.println();
    }
  }

  // ==================== DEBUG FUNCTIONS ====================


  // ==================== LIGHT ANIMATION FUNCTIONS ====================

  void setSolidColor(CRGB* leds, int numLeds, CRGB color) {
    for(int i = 0; i < numLeds; i++) {
      leds[i] = color;
    }
  }

  void setDimPurple(CRGB* leds, int numLeds) {
    setSolidColor(leds, numLeds, PURPLE_COLOR);
  }

  void setPulsingColor(CRGB* leds, int numLeds, CRGB color) {
    float pulse = sin(millis() / PULSE_SPEED);
    int brightness = (int)((pulse + 1.0) * 127);

    for(int i = 0; i < numLeds; i++) {
      leds[i] = color;
      leds[i].nscale8(brightness);
    }
  }

  void setWhiteChase(CRGB* leds, int numLeds, CRGB baseColor) {
    // Create repeating pattern: 5 blue, 5 white, 5 blue, 5 white...
    // Pattern chases continuously along the strip
    int offset = (millis() / CHASE_SPEED);  // Continuously increasing offset

    for(int i = 0; i < numLeds; i++) {
      // Determine position in the 10-LED pattern (5 blue + 5 white)
      int patternPosition = (i + offset) % 10;

      // First 5 positions are blue, next 5 are white
      if(patternPosition < 5) {
        leds[i] = baseColor;
      } else {
        leds[i] = CRGB::White;
      }
    }
  }

  void updateLights() {
    unsigned long stateElapsed = millis() - stateStartTime;
    unsigned long stateDuration = getStateDuration(currentState);

    switch(currentState) {
      case IDLE:
        setDimPurple(redLeds, NUM_LEDS_RED);
        setDimPurple(blueLeds, NUM_LEDS_BLUE);
        break;

      case AUTO:
      case AUTO_PAUSE:
        setSolidColor(redLeds, NUM_LEDS_RED, RED_COLOR);
        setSolidColor(blueLeds, NUM_LEDS_BLUE, BLUE_COLOR);
        break;

      case TRANSITION:
        if(redWonAuto) {
          setWhiteChase(redLeds, NUM_LEDS_RED, RED_COLOR);
          setSolidColor(blueLeds, NUM_LEDS_BLUE, BLUE_COLOR);
        } else {
          setSolidColor(redLeds, NUM_LEDS_RED, RED_COLOR);
          setWhiteChase(blueLeds, NUM_LEDS_BLUE, BLUE_COLOR);
        }
        break;

      case SHIFT_1:
      case SHIFT_2:
      case SHIFT_3:
      case SHIFT_4: {
        int shiftNum = currentState - SHIFT_1;
        bool redActive = redWonAuto ? (shiftNum % 2 == 1) : (shiftNum % 2 == 0);
        bool inWarning = (stateElapsed >= stateDuration - DEACTIVATION_WARNING);

        // Don't pulse during SHIFT_4 warning (smooth transition to ENDGAME)
        bool shouldPulse = inWarning && (currentState != SHIFT_4);

        if(inWarning && !lastWarningState) {
          lastWarningState = true;
        }

        if(redActive) {
          if(shouldPulse) {
            setPulsingColor(redLeds, NUM_LEDS_RED, RED_COLOR);
          } else {
            setSolidColor(redLeds, NUM_LEDS_RED, RED_COLOR);
          }
          setSolidColor(blueLeds, NUM_LEDS_BLUE, CRGB::Black);
        } else {
          setSolidColor(redLeds, NUM_LEDS_RED, CRGB::Black);
          if(shouldPulse) {
            setPulsingColor(blueLeds, NUM_LEDS_BLUE, BLUE_COLOR);
          } else {
            setSolidColor(blueLeds, NUM_LEDS_BLUE, BLUE_COLOR);
          }
        }
        break;
      }

      case ENDGAME:
        setSolidColor(redLeds, NUM_LEDS_RED, RED_COLOR);
        setSolidColor(blueLeds, NUM_LEDS_BLUE, BLUE_COLOR);
        break;

      case MATCH_OVER:
        setSolidColor(redLeds, NUM_LEDS_RED, GREEN_COLOR);
        setSolidColor(blueLeds, NUM_LEDS_BLUE, GREEN_COLOR);
        break;
    }

    FastLED.show();
  }

  // ==================== SETUP ====================
  void setup() {
    Serial.begin(9600);
    delay(500);

    Serial.println(F("FRC Hub Lights"));

    // Initialize DFPlayer
    dfSerial.begin(9600);
    delay(500);
    myMP3.begin(dfSerial);
    myMP3.volume(25);
    delay(100);

    // Initialize IR transmitter and receiver
    IrSender.begin(IR_SEND_PIN);
    IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);

    // Send initial timer commands (reset and stop)
    sendIRCommand(IR_CMD_RESET);
    sendIRCommand(IR_CMD_STOP);

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(SWITCH_RED, INPUT_PULLUP);
    pinMode(SWITCH_BLUE, INPUT_PULLUP);

    FastLED.addLeds<WS2812B, LED_PIN_RED, GRB>(redLeds, NUM_LEDS_RED);
    FastLED.addLeds<WS2812B, LED_PIN_BLUE, GRB>(blueLeds, NUM_LEDS_BLUE);
    FastLED.setBrightness(80);

    if(AUTOSTART) {
      enterState(AUTO);
    } else {
      enterState(IDLE);
    }
  }

  // ==================== MAIN LOOP ====================
  void loop() {
    bool currentButtonState = digitalRead(BUTTON_PIN);

    if(lastButtonState == HIGH && currentButtonState == LOW) {
      if(currentState == IDLE || currentState == MATCH_OVER) {
        enterState(AUTO);
      } else {
        enterState(IDLE);
      }
    }
    lastButtonState = currentButtonState;

    unsigned long stateElapsed = millis() - stateStartTime;
    unsigned long stateDuration = getStateDuration(currentState);

    if(stateElapsed >= stateDuration) {
      MatchState nextState = getNextState(currentState);
      enterState(nextState);
    }

    updateLights();

    // Check for received IR signals
    if (IrReceiver.decode()) {
      Serial.print(F("IR Received: "));

      // Decode and print friendly command names
      switch(IrReceiver.decodedIRData.command) {
        case 0x1F:
          Serial.println(F("Start"));
          break;
        case 0x20:
          Serial.println(F("Stop"));
          break;
        case 0x23:
          Serial.println(F("P1"));
          break;
        case 0x24:
          Serial.println(F("P2"));
          break;
        case 0x25:
          Serial.println(F("P3"));
          break;
        default:
          Serial.print(F("Unknown (0x"));
          Serial.print(IrReceiver.decodedIRData.command, HEX);
          Serial.println(F(")"));
          break;
      }

      IrReceiver.resume();  // Ready for next signal
    }

    delay(10);
  }
