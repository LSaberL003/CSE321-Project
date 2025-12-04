#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ==== Optimized Loop Fall Detector ====
// Algorithm: Impact Detection + Sustained Motion Check
// 1. Detect High G (Impact).
// 2. Monitoring window (5s): Check motion activity in each second.
// 3. If motion detected in >= 3 seconds -> Running/Walking (False Alarm).
// 4. If motion detected in < 3 seconds -> Fall Confirmed (Impact + Stillness).

// ==== DEMO NOTES====
// Definition: System correctness depends on LOGIC and TIMING (Deadline).
// Implementation: Non-blocking Loop using millis() and State Machine.
//    - Sensor: Runs with High Priority to catch impact.
//    - Display: Low Proirity (100ms) to prevent I2C blocking.
// Alternatives: Tried FreeRTOS, but Arduino Uno RAM (2KB) is too small -> Stack Overflow.
// Edge Cases: "Sustained Motion" logic filters out jumping/running false alarms.



// ---- OLED Configuration ----
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---- MPU6050 I2C Address ----
const uint8_t MPU_ADDR = 0x68;

// ---- Pin Assignments ----
const int PIN_LED    = 9;
const int PIN_BUZZER = 8;
const int PIN_BUTTON = 2;

// ---- Fall Detection Parameters ----
const float IMPACT_THRESHOLD_G    = 2.0f;  // Trigger threshold
const float STABILITY_THRESHOLD_G = 1.3f;  // Motion threshold
const unsigned long JUDGE_WINDOW_MS = 5000UL; // Stability check window (5s)
const unsigned long LONG_PRESS_MS   = 2000UL; // Button hold time to reset
const int REQUIRED_ACTIVE_SECONDS   = 3;      // Motion required in X distinct seconds to cancel alarm

// Pre-calculate squares for faster comparison
const float IMPACT_SQ    = IMPACT_THRESHOLD_G * IMPACT_THRESHOLD_G;
const float STABILITY_SQ = STABILITY_THRESHOLD_G * STABILITY_THRESHOLD_G;

// ---- State Machine ----
enum SystemState {
  STATE_MONITORING,    // Normal monitoring
  STATE_JUDGING,       // Impact detected, checking sustained motion
  STATE_ALARM          // Fall confirmed
};

SystemState currentState = STATE_MONITORING;

// ---- State Variables ----
unsigned long impactTime    = 0;
bool btnWasLow              = false;
unsigned long btnPressStart = 0;
bool btnHeld                = false;

// Stability Check Variables
bool motionHistory[5]; // Tracks motion in each of the 5 seconds (0-1s, 1-2s...)

// ---- Display Timer ----
unsigned long lastDisplayTime = 0;
const unsigned long DISPLAY_INTERVAL = 100; // Refresh rate (ms)

// Global variable for display
float globalAMag = 1.0f;

// ---- Function Prototypes ----
void mpuInit();
float readAccelSqMagnitude();

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000); // 400kHz I2C for fast OLED

  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  noTone(PIN_BUZZER);

  pinMode(PIN_BUTTON, INPUT_PULLUP);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  // Splash Screen
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Fall Detect"));
  display.display();
  delay(1000);

  mpuInit();
  Serial.println(F("MPU6050 initialized."));

  // Initial Status Screen
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("MPU6050 + OLED"));
  display.println(F("Monitoring accel..."));
  display.display();
}

void loop() {
  unsigned long now = millis();

  // 1) Sensor Read (Full Speed)
  float aMagSq = readAccelSqMagnitude();

  // 2) Fall Detection Logic (State Machine)
  switch (currentState) {

    // --- State 1: Monitoring ---
    case STATE_MONITORING:
      if (aMagSq > IMPACT_SQ) {
        currentState = STATE_JUDGING;
        impactTime = now;

        // Reset motion history
        for(int i=0; i<5; i++) motionHistory[i] = false;

        Serial.print(F(">> Impact (High G)! Checking 5s motion... G="));
        Serial.println(sqrt(aMagSq));
      }
      break;

    // --- State 2: Judging (5s window) ---
    case STATE_JUDGING:
      {
        unsigned long timeSinceImpact = now - impactTime;

        // A. Record Motion
        // Map current time to a second slot (0, 1, 2, 3, 4)
        int secondIndex = timeSinceImpact / 1000;

        if (secondIndex < 5 && aMagSq > STABILITY_SQ) {
           motionHistory[secondIndex] = true;
           // Optional: Print debug only once per second to avoid spam?
           // Serial.print("Motion in sec "); Serial.println(secondIndex);
        }

        // B. Check Timeout (End of 5s)
        if (timeSinceImpact >= JUDGE_WINDOW_MS) {
          // Count active seconds
          int activeCount = 0;
          Serial.print(F(">> Motion Check: ["));
          for(int i=0; i<5; i++) {
            if (motionHistory[i]) {
              activeCount++;
              Serial.print(F("X"));
            } else {
              Serial.print(F("_"));
            }
          }
          Serial.print(F("] Total: ")); Serial.println(activeCount);

          // Decision
          if (activeCount >= REQUIRED_ACTIVE_SECONDS) {
            // Motion in >= 3 distinct seconds -> Walking/Running
            currentState = STATE_MONITORING;
            Serial.println(F(">> Sustained Motion -> False Alarm. Reset."));
          } else {
            // Motion in < 3 distinct seconds -> Fall Confirmed
            currentState = STATE_ALARM;
            Serial.println(F(">> Little/No Motion -> FALL CONFIRMED! Alarm!"));
          }
        }
      }
      break;

    // --- State 3: Alarm ---
    case STATE_ALARM:
      // Waiting for user reset
      break;
  }

  // 3) Button Logic (Long Press Reset)
  int btnState = digitalRead(PIN_BUTTON); // LOW = pressed

  if (currentState == STATE_ALARM) {
    if (btnState == LOW) {
      if (!btnWasLow) {
        btnWasLow = true;
        btnPressStart = now;
      } else {
        if (now - btnPressStart >= LONG_PRESS_MS) {
          currentState = STATE_MONITORING;
          btnWasLow = false;
          Serial.println(F(">> Alarm canceled by LONG press"));
        }
      }
      btnHeld = true;
    } else {
      btnWasLow = false;
      btnHeld = false;
    }
  } else {
    btnWasLow = false;
    btnHeld = (btnState == LOW);
  }

  // 4) Hardware Output (LED & Buzzer)
  if (currentState == STATE_ALARM) {
    // Siren Effect: Toggle every 300ms
    // High Pitch (2000Hz) + LED ON
    // Low Pitch (1000Hz) + LED OFF (Flashing)
    if ((now / 300) % 2 == 0) {
      digitalWrite(PIN_LED, HIGH);
      tone(PIN_BUZZER, 2000);
    } else {
      digitalWrite(PIN_LED, LOW);
      tone(PIN_BUZZER, 1000);
    }
  } else {
    digitalWrite(PIN_LED, LOW);
    noTone(PIN_BUZZER);
  }

  // 5) UI Update (Throttled)
  if (now - lastDisplayTime >= DISPLAY_INTERVAL) {
    lastDisplayTime = now;

    // Calculate real G for display
    globalAMag = sqrt(aMagSq);

    // Serial Output
    Serial.print(F("Accel |a| = "));
    Serial.print(globalAMag, 2);
    Serial.println(F(" g"));

    // OLED Output
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F("Fall Detector v1"));
    display.println();

    display.print(F("Accel |a|: "));
    display.print(globalAMag, 2);
    display.println(F(" g"));
    display.println();

    if (currentState == STATE_MONITORING) {
      display.println(F("State: MONITORING"));
      display.println(F("Shake to trigger."));

    } else if (currentState == STATE_JUDGING) {
      display.println(F("State: ANALYZING"));

      // Visual feedback of the 5s window
      display.print(F("["));
      unsigned long elapsed = now - impactTime;
      int sec = elapsed / 1000;
      for(int i=0; i<5; i++) {
        if (i < sec) {
            // Past seconds: show if motion was detected
            display.print(motionHistory[i] ? "X" : "_");
        } else if (i == sec) {
            // Current second: blinking cursor
            display.print((elapsed % 500 < 250) ? "?" : " ");
        } else {
            // Future
            display.print(".");
        }
      }
      display.println(F("]"));

    } else if (currentState == STATE_ALARM) {
      display.println(F("State: ALARM!"));
      display.println(F("Hold button 2s"));
      display.println(F("to cancel."));
      if (btnHeld) {
        display.println();
        display.println(F("Button held..."));
      }
    }

    display.display();
  }
}

// ---- Helper Functions ----

void mpuInit() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C);
  Wire.write(0x00); // +/- 2g
  Wire.endTransmission(true);
}

float readAccelSqMagnitude() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, (uint8_t)6, (uint8_t)true);

  if (Wire.available() < 6) return 0.0f;

  int16_t ax_raw = (Wire.read() << 8) | Wire.read();
  int16_t ay_raw = (Wire.read() << 8) | Wire.read();
  int16_t az_raw = (Wire.read() << 8) | Wire.read();

  const float SCALE = 16384.0f;
  float ax = ax_raw / SCALE;
  float ay = ay_raw / SCALE;
  float az = az_raw / SCALE;

  return (ax * ax + ay * ay + az * az);
}