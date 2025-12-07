const int PIN_BUZZER = 8;

void setup() {
  pinMode(PIN_BUZZER, OUTPUT);
}

void loop() {
  // Simulate Alarm Siren
  tone(PIN_BUZZER, 2000); // High pitch 2kHz
  delay(300);

  tone(PIN_BUZZER, 1000); // Low pitch 1kHz
  delay(300);


}