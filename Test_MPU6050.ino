#include <Wire.h>

const int MPU_ADDR = 0x68;

void setup() {
  Serial.begin(9600);
  Wire.begin();

  // 1. Wake up MPU6050 (It starts in sleep mode)
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); // PWR_MGMT_1 register
  Wire.write(0);    // set to zero (wakes up the MPU-6050)
  Wire.endTransmission(true);

  Serial.println("MPU6050 Ready. Reading values...");
  delay(1000);
}

void loop() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B); // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true); // request a total of 6 registers

  if (Wire.available() < 6) {
    Serial.println("I2C Error: Data not available");
    return;
  }

  // Read high and low bytes and combine them
  int16_t AcX = Wire.read()<<8 | Wire.read();
  int16_t AcY = Wire.read()<<8 | Wire.read();
  int16_t AcZ = Wire.read()<<8 | Wire.read();

  // Convert to rough G-force (Assuming default +/- 2g range, 16384 LSB/g)
  float gX = AcX / 16384.0;
  float gY = AcY / 16384.0;
  float gZ = AcZ / 16384.0;

  Serial.print("Accel X: "); Serial.print(gX);
  Serial.print(" | Y: "); Serial.print(gY);
  Serial.print(" | Z: "); Serial.println(gZ);

  delay(500);
}