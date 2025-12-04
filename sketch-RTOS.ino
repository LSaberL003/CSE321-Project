#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---- FreeRTOS on Arduino Uno ----
#include <Arduino_FreeRTOS.h>
#include <queue.h>

#define USE_OLED 1

// ---- OLED configuration ----
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET   -1

#if USE_OLED
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#endif

// ---- MPU6050 I2C address ----
const uint8_t MPU_ADDR = 0x68;  // AD0 → GND

// ---- Pin assignments ----
const int PIN_LED    = 9;
const int PIN_BUZZER = 8;
const int PIN_BUTTON = 2;   // D2 to GND, INPUT_PULLUP

// ---- Fall detection parameters ----
const float IMPACT_THRESHOLD_G    = 1.8f;     // impact threshold
const unsigned long LONG_PRESS_MS = 2000UL;   // 2 s long press

// ---- RTOS timing (ms) ----
const TickType_t IMU_PERIOD_MS    = 50;       // IMU sample + OLED update
const TickType_t ALARM_PERIOD_MS  = 20;       // alarm/button task tick

// ---- Shared alarm state ----
volatile bool alarmActive = false;   // Alarm ON/OFF, written by TaskAlarm

// ---- Event queue ----
enum EventType : uint8_t {
  EV_NONE = 0,
  EV_IMPACT
};

QueueHandle_t xEventQueue = NULL;

// ---- Function prototypes ----
void mpuInit();
float readAccelMagnitudeG();

// RTOS tasks
void TaskIMU(void *pvParameters);
void TaskAlarm(void *pvParameters);

// ========================================================
//                        setup()
// ========================================================
void setup() {
  Serial.begin(115200);
  Wire.begin();

  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  noTone(PIN_BUZZER);

  pinMode(PIN_BUTTON, INPUT_PULLUP);

#if USE_OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Fall Detect"));
  display.display();
  delay(1000);
#endif

  // Init MPU6050
  mpuInit();
  Serial.println(F("MPU6050 initialized."));

#if USE_OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("MPU6050 + FreeRTOS"));
  display.println(F("Monitoring accel..."));
  display.display();
#endif

  // ---- Create event queue ----
  xEventQueue = xQueueCreate(4, sizeof(EventType));
  if (xEventQueue == NULL) {
    Serial.println(F("Failed to create event queue!"));
    for (;;);
  }

  // ---- Create tasks ----
  BaseType_t ok;

  ok = xTaskCreate(
    TaskIMU,
    "IMU",
    128,      // 128 words
    NULL,
    2,        // higher priority
    NULL
  );
  if (ok != pdPASS) {
    Serial.println(F("Failed to create TaskIMU"));
  }

  ok = xTaskCreate(
    TaskAlarm,
    "ALARM",
    128,      // 128 words
    NULL,
    1,        // lower priority
    NULL
  );
  if (ok != pdPASS) {
    Serial.println(F("Failed to create TaskAlarm"));
  }


}

void loop() {
}

// ========================================================
//                  Task: IMU + OLED + detection
// ========================================================
void TaskIMU(void *pvParameters) {
  (void) pvParameters;

  TickType_t lastWake = xTaskGetTickCount();

  for (;;) {
    // 1) Read acceleration magnitude |a|
    float aMag = readAccelMagnitudeG();

    // Debug print
    Serial.print(F("Accel |a| = "));
    Serial.print(aMag, 2);
    Serial.println(F(" g"));

    // 2) Impact detection → send event to alarm task
    if (!alarmActive && aMag > IMPACT_THRESHOLD_G) {
      EventType ev = EV_IMPACT;
      xQueueSend(xEventQueue, &ev, 0);
      Serial.println(F(">> Impact detected, EV_IMPACT sent"));
    }

#if USE_OLED
    // 3) Update OLED
    bool localAlarm = alarmActive;

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F("Fall Detector RTOS"));
    display.println();

    display.print(F("Accel |a|: "));
    display.print(aMag, 2);
    display.println(F(" g"));
    display.println();

    if (!localAlarm) {
      display.println(F("State: MONITORING"));
      display.println(F("Shake / move to"));
      display.println(F("trigger alarm."));
    } else {
      display.println(F("State: ALARM!"));
      display.println(F("Hold button 2s"));
      display.println(F("to cancel."));
    }

    display.display();
#endif

    // 4) Periodic delay to get fixed sampling period
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(IMU_PERIOD_MS));
  }
}

// ========================================================
//             Task: Alarm (LED + buzzer + button)
// ========================================================
void TaskAlarm(void *pvParameters) {
  (void) pvParameters;

  bool localAlarm = false;

  bool btnWasLow = false;
  unsigned long btnPressStart = 0;

  TickType_t lastWake = xTaskGetTickCount();

  for (;;) {
    // 1) Check queue for new impact events (non-blocking)
    EventType ev = EV_NONE;
    if (xQueueReceive(xEventQueue, &ev, 0) == pdPASS) {
      if (ev == EV_IMPACT) {
        localAlarm = true;
        Serial.println(F("ALARM: received EV_IMPACT → ON"));
      }
    }

    // 2) Handle button long-press to cancel alarm
    int btnState = digitalRead(PIN_BUTTON); // LOW = pressed
    unsigned long nowMs = millis();

    if (localAlarm) {
      if (btnState == LOW) {
        if (!btnWasLow) {
          btnWasLow = true;
          btnPressStart = nowMs;
        } else {
          if (nowMs - btnPressStart >= LONG_PRESS_MS) {
            localAlarm = false;
            btnWasLow = false;
            Serial.println(F("ALARM: long press → OFF"));
          }
        }
      } else {
        btnWasLow = false;
      }
    } else {
      btnWasLow = false;
    }

    // 3) Drive LED + buzzer
    if (localAlarm) {
      digitalWrite(PIN_LED, HIGH);
      tone(PIN_BUZZER, 2000);  // 2 kHz tone
    } else {
      digitalWrite(PIN_LED, LOW);
      noTone(PIN_BUZZER);
    }

    alarmActive = localAlarm;

    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(ALARM_PERIOD_MS));
  }
}

// ========================================================
//                  MPU6050 helper functions
// ========================================================
void mpuInit() {
  // Wake up the MPU6050
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);   // PWR_MGMT_1
  Wire.write(0);      // wake up
  Wire.endTransmission(true);

  // Set accelerometer full-scale range to ±2 g
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C);   // ACCEL_CONFIG
  Wire.write(0x00);   // ±2 g
  Wire.endTransmission(true);
}

// Read acceleration magnitude |a| in g-units
float readAccelMagnitudeG() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);  // ACCEL_XOUT_H
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, (uint8_t)6, (uint8_t)true);

  if (Wire.available() < 6) {
    return 0.0f;
  }

  int16_t ax_raw = (Wire.read() << 8) | Wire.read();
  int16_t ay_raw = (Wire.read() << 8) | Wire.read();
  int16_t az_raw = (Wire.read() << 8) | Wire.read();

  const float SCALE = 16384.0f;  // LSB/g for ±2 g
  float ax = ax_raw / SCALE;
  float ay = ay_raw / SCALE;
  float az = az_raw / SCALE;

  float mag = sqrt(ax * ax + ay * ay + az * az);
  return mag;
}
