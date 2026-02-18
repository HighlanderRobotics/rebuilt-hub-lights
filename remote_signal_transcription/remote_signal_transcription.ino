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

  IRData &data = IrReceiver.decodedIRData;

  // Print protocol name
  Serial.print(F("Protocol : "));
  Serial.println(getProtocolString(data.protocol));

  // Print decoded value (hex and decimal)
  Serial.print(F("Value    : 0x"));
  Serial.print(data.decodedRawData, HEX);
  Serial.print(F("  ("));
  Serial.print(data.decodedRawData, DEC);
  Serial.println(F(")"));

  // Print address and command fields (useful for NEC / Samsung protocols)
  Serial.print(F("Address  : 0x"));
  Serial.println(data.address, HEX);
  Serial.print(F("Command  : 0x"));
  Serial.println(data.command, HEX);

  // Print raw pulse timings (useful if protocol shows as UNKNOWN)
  Serial.print(F("Raw ("));
  Serial.print(IrReceiver.decodedIRData.rawDataPtr->rawlen - 1);
  Serial.print(F(" pulses): "));
  for (uint16_t i = 1; i < IrReceiver.decodedIRData.rawDataPtr->rawlen; i++) {
    uint16_t us = IrReceiver.decodedIRData.rawDataPtr->rawbuf[i] * MICROS_PER_TICK;
    if (i % 2 == 1) {
      Serial.print(F("+"));
    } else {
      Serial.print(F("-"));
    }
    Serial.print(us);
    Serial.print(F(" "));
  }
  Serial.println();
  Serial.println(F("---"));

  IrReceiver.resume();
}
