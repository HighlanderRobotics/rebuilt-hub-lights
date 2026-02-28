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
  #define IR_SEND_PIN 3
  #define DFPLAYER_RX 10
  #define DFPLAYER_TX 11

  // ==================== LED CONFIGURATION ====================
  #define NUM_LEDS_RED 640       // Optimized for Uno memory
  #define NUM_LEDS_BLUE 640      // Use Arduino Mega for more LEDs

  // ==================== DEBUG CONFIGURATION ====================
  #define AUTOSTART false       // Skip IDLE, start match immediately on power-up
  #define FORCE_BLUE_AUTO false // Force blue to win auto regardless of physical switch
  #define DEBOUNCE_TIME 80     // Button debounce threshold in ms — prevents EMI false triggers
  #define STATUS_INTERVAL 2000 // Periodic status print interval in ms

  // ==================== TIMING CONSTANTS (milliseconds) ====================
  #define AUTO_DURATION 20000
  #define AUTO_PAUSE_DURATION 3000
  #define TRANSITION_DURATION 10500
  #define SHIFT_DURATION 25400
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
  #define IR_ADDRESS 0x22

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
  bool lastWarningState = false;
  unsigned long lastButtonPressTime = 0;
  unsigned long lastStatusPrint = 0;
  int currentAudio = 0;

  CRGB redLeds[NUM_LEDS_RED];
  CRGB blueLeds[NUM_LEDS_BLUE];

  SoftwareSerial dfSerial(DFPLAYER_RX, DFPLAYER_TX);
  DFPlayerMini_Fast myMP3;

  // ==================== DEBUG FUNCTIONS ====================

  void printStateName(MatchState state) {
    switch(state) {
      case IDLE:       Serial.print(F("IDLE"));       break;
      case AUTO:       Serial.print(F("AUTO"));       break;
      case AUTO_PAUSE: Serial.print(F("AUTO_PAUSE")); break;
      case TRANSITION: Serial.print(F("TRANSITION")); break;
      case SHIFT_1:    Serial.print(F("SHIFT_1"));    break;
      case SHIFT_2:    Serial.print(F("SHIFT_2"));    break;
      case SHIFT_3:    Serial.print(F("SHIFT_3"));    break;
      case SHIFT_4:    Serial.print(F("SHIFT_4"));    break;
      case ENDGAME:    Serial.print(F("ENDGAME"));    break;
      case MATCH_OVER: Serial.print(F("MATCH_OVER")); break;
      default:         Serial.print(F("UNKNOWN"));    break;
    }
  }

  void printAudioName(int fileNum) {
    switch(fileNum) {
      case AUDIO_START:   Serial.print(F("match start"));    break;
      case AUDIO_WARNING: Serial.print(F("endgame warning")); break;
      case AUDIO_RESUME:  Serial.print(F("teleop begins"));  break;
      case AUDIO_END:     Serial.print(F("match end"));      break;
      default:            Serial.print(F("none"));           break;
    }
  }

  void printHubColor(bool redHub) {
    switch(currentState) {
      case IDLE:
        Serial.print(F("dim purple"));
        break;
      case AUTO:
      case AUTO_PAUSE:
        Serial.print(redHub ? F("red") : F("blue"));
        break;
      case TRANSITION:
        if(redHub) Serial.print(redWonAuto ? F("white chase") : F("red"));
        else       Serial.print(redWonAuto ? F("blue") : F("white chase"));
        break;
      case SHIFT_1: case SHIFT_2: case SHIFT_3: case SHIFT_4: {
        int shiftNum = currentState - SHIFT_1;
        bool redActive = redWonAuto ? (shiftNum % 2 == 1) : (shiftNum % 2 == 0);
        bool active = redHub ? redActive : !redActive;
        unsigned long elapsed = millis() - stateStartTime;
        bool inWarning = elapsed >= SHIFT_DURATION - DEACTIVATION_WARNING;
        bool shouldPulse = inWarning && (currentState != SHIFT_4);
        if(!active) { Serial.print(F("off")); break; }
        if(shouldPulse) Serial.print(redHub ? F("pulsing red") : F("pulsing blue"));
        else            Serial.print(redHub ? F("red") : F("blue"));
        break;
      }
      case ENDGAME:
        Serial.print(redHub ? F("red") : F("blue"));
        break;
      case MATCH_OVER:
        Serial.print(F("green"));
        break;
    }
  }

  void printPeriodicStatus() {
    unsigned long now = millis();
    if(now - lastStatusPrint < STATUS_INTERVAL) return;
    lastStatusPrint = now;

    unsigned long elapsed = now - stateStartTime;
    unsigned long duration = 0;
    switch(currentState) {
      case AUTO: duration = AUTO_DURATION; break;
      case AUTO_PAUSE: duration = AUTO_PAUSE_DURATION; break;
      case TRANSITION: duration = TRANSITION_DURATION; break;
      case SHIFT_1: case SHIFT_2: case SHIFT_3: case SHIFT_4: duration = SHIFT_DURATION; break;
      case ENDGAME: duration = ENDGAME_DURATION; break;
      default: break;
    }

    Serial.print(F("["));
    Serial.print(now / 1000);
    Serial.print(F("s] "));
    printStateName(currentState);
    Serial.print(F(" | "));
    Serial.print(elapsed / 1000);
    Serial.print(F("s"));
    if(duration > 0) { Serial.print(F("/")); Serial.print(duration / 1000); Serial.print(F("s")); }
    Serial.print(F(" | Red: ")); printHubColor(true);
    Serial.print(F("  Blue: ")); printHubColor(false);
    Serial.print(F(" | Audio: ")); printAudioName(currentAudio);
    Serial.println();
  }

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


  void enterState(MatchState newState) {
    // Log the transition
    Serial.print(F("["));
    Serial.print(millis() / 1000);
    Serial.print(F("s] "));
    printStateName(currentState);
    Serial.print(F(" -> "));
    printStateName(newState);
    Serial.println();

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
      currentAudio = soundFile;
      Serial.print(F("  Audio: "));
      printAudioName(soundFile);
      Serial.println();
    }

    // Read auto-winner switch
    if(newState == TRANSITION) {
      if(AUTOSTART || FORCE_BLUE_AUTO) {
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
      Serial.print(F("  Auto winner: "));
      Serial.println(redWonAuto ? F("RED") : F("BLUE"));
    }

  }

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
    unsigned long offset = (millis() / CHASE_SPEED);  // Continuously increasing offset

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
    Serial.println(F("=== FRC Practice Hub Lights - REBUILT ==="));
    Serial.print(F("LEDs: "));
    Serial.print(NUM_LEDS_RED);
    Serial.print(F(" red / "));
    Serial.print(NUM_LEDS_BLUE);
    Serial.println(F(" blue"));
    Serial.print(F("AUTOSTART: "));
    Serial.println(AUTOSTART ? F("ON (blue wins auto)") : F("OFF"));
    Serial.println(F("========================================="));

    // Initialize DFPlayer
    dfSerial.begin(9600);
    delay(500);
    myMP3.begin(dfSerial);
    myMP3.volume(25);
    delay(100);

    // Initialize IR transmitter
    IrSender.begin(IR_SEND_PIN);

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
      unsigned long now = millis();
      if(now - lastButtonPressTime >= DEBOUNCE_TIME) {
        lastButtonPressTime = now;

        // IR fires first, based on actual game state
        if(currentState == IDLE || currentState == MATCH_OVER) {
          IrSender.sendNEC(IR_ADDRESS, 0x1F, 0);
          Serial.println(F("IR -> 0x1F"));
        } else {
          IrSender.sendNEC(IR_ADDRESS, 0x20, 0);
          Serial.println(F("IR -> 0x20"));
          delay(1000);
          IrSender.sendNEC(IR_ADDRESS, 0x2F, 0);
          Serial.println(F("IR -> 0x2F"));
          delay(1000);
          IrSender.sendNEC(IR_ADDRESS, 0x2F, 0);
          Serial.println(F("IR -> 0x2F"));
        }

        // Match state change after IR
        if(currentState == IDLE || currentState == MATCH_OVER) {
          Serial.println(F(">>> BTN: START"));
          enterState(AUTO);
        } else {
          Serial.println(F(">>> BTN: STOP"));
          enterState(IDLE);
        }
      } else {
        Serial.print(F(">>> BTN: bounce ignored ("));
        Serial.print(now - lastButtonPressTime);
        Serial.println(F("ms)"));
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
    printPeriodicStatus();
    delay(10);
  }
