#include <Wire.h> 
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---- OLED configuration ----
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// No dedicated reset pin on this module, so use -1
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---- MPU6050 I2C address ----
const uint8_t MPU_ADDR = 0x68;  // AD0 tied to GND → address 0x68

// ---- Pin assignments ----
const int PIN_LED    = 9;
const int PIN_BUZZER = 8;
const int PIN_BUTTON = 2;   // Button between D2 and GND (using INPUT_PULLUP)

// ---- Fall detection parameters ----
const float IMPACT_THRESHOLD_G    = 1.8f;      // Impact threshold in g-units (tune empirically)
const unsigned long LONG_PRESS_MS = 2000UL;   // Hold button for 2s to cancel alarm

// ---- Alarm & button state ----
bool alarmActive        = false;  // True while the alarm is active
bool btnWasLow          = false;  // Button state in previous loop iteration
unsigned long btnPressStart = 0;  // Timestamp when button was first pressed

// ---- Function prototypes ----
void mpuInit();
float readAccelMagnitudeG();

void setup() {
  Serial.begin(115200);
  Wire.begin();

  // Configure LED, buzzer and button
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  noTone(PIN_BUZZER);

  // Internal pull-up: not pressed = HIGH, pressed = LOW
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Hard stop if OLED init fails
  }

  // Splash screen
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Fall Detect"));
  display.display();
  delay(1000);

  // Initialize MPU6050
  mpuInit();
  Serial.println("MPU6050 initialized.");

  // Initial status screen
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("MPU6050 + OLED"));
  display.println(F("Monitoring accel..."));
  display.display();
}

void loop() {
  // 1) Read acceleration magnitude |a|
  float aMag = readAccelMagnitudeG();
  unsigned long now = millis();

  // Debug output over serial
  Serial.print("Accel |a| = ");
  Serial.print(aMag, 2);
  Serial.println(" g");

  // 2) Impact detection: NORMAL → ALARM
  if (!alarmActive && aMag > IMPACT_THRESHOLD_G) {
    alarmActive = true;
    Serial.print(">> Impact detected, ALARM ON, aMag = ");
    Serial.println(aMag, 2);
  }

  // 3) Long-press logic for canceling alarm (only active while alarmActive = true)
  int btnState = digitalRead(PIN_BUTTON); // LOW = pressed, HIGH = released
  bool btnHeld = false;

  if (alarmActive) {
    if (btnState == LOW) {
      if (!btnWasLow) {
        // Button was just pressed
        btnWasLow = true;
        btnPressStart = now;
      } else {
        // Button is still pressed, check duration
        if (now - btnPressStart >= LONG_PRESS_MS) {
          alarmActive = false;     // Cancel alarm
          btnWasLow   = false;     // Reset edge-detection state
          Serial.println(">> Alarm canceled by LONG press");
        }
      }
      btnHeld = true;
    } else {
      // Button released before long-press threshold
      btnWasLow = false;
      btnHeld   = false;
    }
  } else {
    // Not in alarm state → make sure button state is clean
    btnWasLow = false;
    btnHeld   = (btnState == LOW);
  }

  // 4) Drive LED and buzzer based on alarm state
  if (alarmActive) {
    digitalWrite(PIN_LED, HIGH);
    tone(PIN_BUZZER, 2000);   // 2 kHz alarm tone
  } else {
    digitalWrite(PIN_LED, LOW);
    noTone(PIN_BUZZER);
  }

  // 5) Update OLED with current acceleration and state
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Fall Detector v1"));
  display.println();

  display.print(F("Accel |a|: "));
  display.print(aMag, 2);
  display.println(F(" g"));
  display.println();

  if (!alarmActive) {
    display.println(F("State: MONITORING"));
    display.println(F("Shake to trigger."));
  } else {
    display.println(F("State: ALARM!"));
    display.println(F("Hold button 2s"));
    display.println(F("to cancel."));
    if (btnHeld) {
      display.println();
      display.println(F("Button held..."));
    }
  }

  display.display();

  delay(50); // UI update rate
}

// ---- MPU6050 initialization ----
void mpuInit() {
  // Wake up the MPU6050
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);      // PWR_MGMT_1 register
  Wire.write(0);         // Set to 0 → wake up device
  Wire.endTransmission(true);

  // Explicitly set accelerometer full-scale range to ±2 g
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C);      // ACCEL_CONFIG register
  Wire.write(0x00);      // ±2 g
  Wire.endTransmission(true);
}

// ---- Read acceleration magnitude |a| in g-units ----
float readAccelMagnitudeG() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);  // Starting register: ACCEL_XOUT_H
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, (uint8_t)6, (uint8_t)true); // Read X, Y, Z (6 bytes)

  if (Wire.available() < 6) {
    // I2C error or incomplete read
    return 0.0f;
  }

  int16_t ax_raw = (Wire.read() << 8) | Wire.read();
  int16_t ay_raw = (Wire.read() << 8) | Wire.read();
  int16_t az_raw = (Wire.read() << 8) | Wire.read();

  // For ±2 g, sensitivity is 16384 LSB/g
  const float SCALE = 16384.0f;
  float ax = ax_raw / SCALE;
  float ay = ay_raw / SCALE;
  float az = az_raw / SCALE;

  float mag = sqrt(ax * ax + ay * ay + az * az);
  return mag;
}
