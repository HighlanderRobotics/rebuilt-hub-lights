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
  #define NUM_LEDS_RED 200       // Optimized for Uno memory
  #define NUM_LEDS_BLUE 200      // Use Arduino Mega for more LEDs

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
  #define IR_ADDRESS 0x22
  #define IR_CMD_START 0x1F

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

    // Send IR command when match starts
    if(newState == AUTO) {
      IrSender.sendNEC(IR_ADDRESS, IR_CMD_START, 0);
      delay(100);
    }

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
    delay(10);
  }
