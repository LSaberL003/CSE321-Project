const int PIN_LED = 9;

void setup() {
  pinMode(PIN_LED, OUTPUT);
}

void loop() {
  digitalWrite(PIN_LED, HIGH); // Turn ON
  delay(500);
  digitalWrite(PIN_LED, LOW);  // Turn OFF
  delay(500);
}