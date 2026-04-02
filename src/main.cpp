#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <MPU6050.h>

namespace {

constexpr uint32_t kSerialBaud = 115200;
constexpr uint32_t kStartupDelayMs = 2000;
constexpr uint32_t kLoopDelayMs = 1000;
constexpr uint8_t kBme280Address = 0x77;
constexpr float kSeaLevelPressureHpa = 1013.25f;

Adafruit_BME280 bme;
MPU6050 mpu;

[[noreturn]] void haltWithError(const char *message) {
  Serial.println(message);
  while (true) {
    delay(1000);
  }
}

}  // namespace

void setup() {
  Serial.begin(kSerialBaud);
  delay(kStartupDelayMs);

  Serial.println("Teensy 4.1 Flight Computer Sensor Test");

  Wire.begin();
  Wire.setClock(400000);

  mpu.initialize();
  if (!mpu.testConnection()) {
    haltWithError("ERROR: Could not find MPU6050");
  }

  Serial.println("MPU6050 initialized");

  if (!bme.begin(kBme280Address)) {
    haltWithError("ERROR: Could not find BME280");
  }

  Serial.println("BME280 initialized");
  Serial.println("time_ms,ax_g,ay_g,az_g,gx_deg_s,gy_deg_s,gz_deg_s,temp_C,pressure_Pa,altitude_m");
}

void loop() {
  const unsigned long time_ms = millis();

  int16_t ax_raw = 0;
  int16_t ay_raw = 0;
  int16_t az_raw = 0;
  int16_t gx_raw = 0;
  int16_t gy_raw = 0;
  int16_t gz_raw = 0;

  mpu.getMotion6(&ax_raw, &ay_raw, &az_raw, &gx_raw, &gy_raw, &gz_raw);

  const float ax_g = ax_raw / 16384.0f;
  const float ay_g = ay_raw / 16384.0f;
  const float az_g = az_raw / 16384.0f;

  const float gx_deg_s = gx_raw / 131.0f;
  const float gy_deg_s = gy_raw / 131.0f;
  const float gz_deg_s = gz_raw / 131.0f;

  const float temp_C = bme.readTemperature();
  const float pressure_Pa = bme.readPressure();
  const float altitude_m = bme.readAltitude(kSeaLevelPressureHpa);

  Serial.print(time_ms);
  Serial.print(",");
  Serial.print(ax_g, 4);
  Serial.print(",");
  Serial.print(ay_g, 4);
  Serial.print(",");
  Serial.print(az_g, 4);
  Serial.print(",");
  Serial.print(gx_deg_s, 4);
  Serial.print(",");
  Serial.print(gy_deg_s, 4);
  Serial.print(",");
  Serial.print(gz_deg_s, 4);
  Serial.print(",");
  Serial.print(temp_C, 2);
  Serial.print(",");
  Serial.print(pressure_Pa, 2);
  Serial.print(",");
  Serial.println(altitude_m, 2);

  delay(kLoopDelayMs);
}
