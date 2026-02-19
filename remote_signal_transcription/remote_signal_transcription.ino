#define IR_PIN 3

void setup() {
  Serial.begin(9600);
  pinMode(IR_PIN, INPUT);
  Serial.println(F("Ready"));
}

void loop() {
  if (digitalRead(IR_PIN) == HIGH) {
    Serial.println(F("high"));
    delay(500);
  }
}
