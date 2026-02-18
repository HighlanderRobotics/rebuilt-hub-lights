/*
 * IR Signal Transcription
 *
 * Receives IR signals and prints the decoded protocol, value, and raw pulse
 * timings to Serial. Use this to identify what your timer controller sends.
 *
 * Hardware:
 *   - IR receiver module (e.g. VS1838B, TSOP38238) signal pin → Pin 2
 *   - VCC → 5V, GND → GND
 *
 * Library required: IRremote (by shirriff / z3t0)
 *   Install via Arduino Library Manager: search "IRremote"
 *   Tested with IRremote v4.x
 *
 * Serial Monitor: 115200 baud
 */

#include <IRremote.hpp>

#define IR_RECEIVE_PIN 2

void setup() {
  Serial.begin(115200);
  Serial.println(F("IR Signal Transcription"));
  Serial.println(F("Waiting for signal..."));
  Serial.println();

  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
}

void loop() {
  if (!IrReceiver.decode()) {
    return;
  }

  // Print decoded result (protocol, address, command, value)
  IrReceiver.printIRResultShort(&Serial);

  // Print raw pulse timings — useful when protocol shows as UNKNOWN
  IrReceiver.printIRResultRawFormatted(&Serial, true);

  Serial.println(F("---"));

  IrReceiver.resume();
}
