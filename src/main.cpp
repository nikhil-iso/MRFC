#include <Arduino.h>
#include <Wire.h>
#include <SD.h>
#include <Adafruit_BME280.h>
#include <MPU6050.h>
#include <math.h>
#include <stdio.h>

namespace {

constexpr uint32_t kSerialBaud = 115200;
constexpr uint32_t kStartupDelayMs = 2000;
constexpr uint32_t kLoopPeriodMs = 50;
constexpr uint32_t kCalibrationDurationMs = 3000;
constexpr uint32_t kCalibrationSampleIntervalMs = 50;
constexpr uint8_t kBme280Address = 0x77;
constexpr uint8_t kMpuAccelRange = MPU6050_ACCEL_FS_16;
constexpr uint8_t kMpuGyroRange = MPU6050_GYRO_FS_1000;
constexpr float kAltitudeFilterAlpha = 0.2f;
constexpr float kAccelFilterAlpha = 0.2f;
constexpr size_t kAccelAverageWindowSize = 5;
constexpr uint32_t kSdFlushIntervalSamples = 20;
constexpr size_t kLogFileNameLength = 13;

constexpr float accelScaleLsbPerG(uint8_t range) {
  switch (range) {
    case MPU6050_ACCEL_FS_2:
      return 16384.0f;
    case MPU6050_ACCEL_FS_4:
      return 8192.0f;
    case MPU6050_ACCEL_FS_8:
      return 4096.0f;
    case MPU6050_ACCEL_FS_16:
    default:
      return 2048.0f;
  }
}

constexpr float gyroScaleLsbPerDps(uint8_t range) {
  switch (range) {
    case MPU6050_GYRO_FS_250:
      return 131.0f;
    case MPU6050_GYRO_FS_500:
      return 65.5f;
    case MPU6050_GYRO_FS_1000:
      return 32.8f;
    case MPU6050_GYRO_FS_2000:
    default:
      return 16.4f;
  }
}

constexpr float kAccelScaleLsbPerG = accelScaleLsbPerG(kMpuAccelRange);
constexpr float kGyroScaleLsbPerDps = gyroScaleLsbPerDps(kMpuGyroRange);

Adafruit_BME280 bme;
MPU6050 mpu;
float pressureBaselinePa = 0.0f;
float altitudeFilteredM = 0.0f;
float totalAccelFilteredG = 0.0f;
bool altitudeFilterInitialized = false;
bool totalAccelFilterInitialized = false;
File telemetryLogFile;
bool sdLoggingEnabled = false;
uint32_t samplesSinceFlush = 0;
char telemetryLogFileName[kLogFileNameLength] = {};

struct TelemetrySample {
  unsigned long timeMs = 0;
  float axG = 0.0f;
  float ayG = 0.0f;
  float azG = 0.0f;
  float gxDegS = 0.0f;
  float gyDegS = 0.0f;
  float gzDegS = 0.0f;
  float tempC = 0.0f;
  float pressurePa = 0.0f;
  float pressureBaselinePa = 0.0f;
  float altitudeRelM = 0.0f;
  float altitudeLpfM = 0.0f;
  float totalAccelG = 0.0f;
  float totalAccelLpfG = 0.0f;
};

struct MovingAverageFilter {
  float samples[kAccelAverageWindowSize] = {};
  size_t index = 0;
  size_t count = 0;
  float sum = 0.0f;

  float update(float sample) {
    if (count < kAccelAverageWindowSize) {
      samples[index] = sample;
      sum += sample;
      ++count;
    } else {
      sum -= samples[index];
      samples[index] = sample;
      sum += sample;
    }

    index = (index + 1) % kAccelAverageWindowSize;
    return sum / static_cast<float>(count);
  }
};

MovingAverageFilter totalAccelAverageFilter;

[[noreturn]] void haltWithError(const char *message) {
  Serial.println(message);
  while (true) {
    delay(1000);
  }
}

bool writeCsvHeader(Print &output) {
  return output.println(
             "time_ms,ax_g,ay_g,az_g,gx_deg_s,gy_deg_s,gz_deg_s,temp_C,"
             "pressure_Pa,pressure_baseline_Pa,altitude_rel_m,"
             "altitude_lpf_m,a_total_g,a_total_lpf_g") > 0;
}

bool writeCsvRow(Print &output, const TelemetrySample &sample) {
  bool success = true;

  success &= output.print(sample.timeMs) > 0;
  success &= output.print(",") > 0;
  success &= output.print(sample.axG, 4) > 0;
  success &= output.print(",") > 0;
  success &= output.print(sample.ayG, 4) > 0;
  success &= output.print(",") > 0;
  success &= output.print(sample.azG, 4) > 0;
  success &= output.print(",") > 0;
  success &= output.print(sample.gxDegS, 4) > 0;
  success &= output.print(",") > 0;
  success &= output.print(sample.gyDegS, 4) > 0;
  success &= output.print(",") > 0;
  success &= output.print(sample.gzDegS, 4) > 0;
  success &= output.print(",") > 0;
  success &= output.print(sample.tempC, 2) > 0;
  success &= output.print(",") > 0;
  success &= output.print(sample.pressurePa, 2) > 0;
  success &= output.print(",") > 0;
  success &= output.print(sample.pressureBaselinePa, 2) > 0;
  success &= output.print(",") > 0;
  success &= output.print(sample.altitudeRelM, 2) > 0;
  success &= output.print(",") > 0;
  success &= output.print(sample.altitudeLpfM, 2) > 0;
  success &= output.print(",") > 0;
  success &= output.print(sample.totalAccelG, 4) > 0;
  success &= output.print(",") > 0;
  success &= output.println(sample.totalAccelLpfG, 4) > 0;

  return success;
}

float applyLowPassFilter(float sample, float alpha, float &state,
                         bool &initialized) {
  if (!initialized) {
    state = sample;
    initialized = true;
  } else {
    state += alpha * (sample - state);
  }

  return state;
}

float computeRelativeAltitudeMeters(float pressurePa) {
  return 44330.0f * (1.0f - powf(pressurePa / pressureBaselinePa, 0.1903f));
}

void configureMpuRanges() {
  mpu.setFullScaleAccelRange(kMpuAccelRange);
  mpu.setFullScaleGyroRange(kMpuGyroRange);
}

void disableSdLogging(const char *message) {
  Serial.println(message);

  if (telemetryLogFile) {
    telemetryLogFile.close();
  }

  sdLoggingEnabled = false;
  samplesSinceFlush = 0;
  telemetryLogFileName[0] = '\0';
}

bool findNextLogFileName(char *buffer, size_t bufferLength) {
  for (uint8_t index = 0; index < 100; ++index) {
    if (snprintf(buffer, bufferLength, "FLIGHT%02u.CSV", index) <= 0) {
      return false;
    }

    if (!SD.exists(buffer)) {
      return true;
    }
  }

  return false;
}

void initializeSdLogging() {
  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println(
        "WARNING: SD card initialization failed. Continuing with serial "
        "telemetry only.");
    return;
  }

  if (!findNextLogFileName(telemetryLogFileName, sizeof(telemetryLogFileName))) {
    Serial.println(
        "WARNING: No free FLIGHTNN.CSV filename found on SD card. Continuing "
        "with serial telemetry only.");
    return;
  }

  telemetryLogFile = SD.open(telemetryLogFileName, FILE_WRITE);
  if (!telemetryLogFile) {
    Serial.println(
        "WARNING: Could not open SD log file. Continuing with serial telemetry "
        "only.");
    telemetryLogFileName[0] = '\0';
    return;
  }

  telemetryLogFile.clearWriteError();
  if (!writeCsvHeader(telemetryLogFile) || telemetryLogFile.getWriteError() != 0) {
    disableSdLogging(
        "WARNING: Failed to write CSV header to SD log file. Continuing with "
        "serial telemetry only.");
    return;
  }

  telemetryLogFile.clearWriteError();
  telemetryLogFile.flush();
  if (telemetryLogFile.getWriteError() != 0) {
    disableSdLogging(
        "WARNING: Failed to flush SD log header. Continuing with serial "
        "telemetry only.");
    return;
  }

  sdLoggingEnabled = true;
  samplesSinceFlush = 0;

  Serial.print("SD logging to ");
  Serial.println(telemetryLogFileName);
}

void logTelemetryToSd(const TelemetrySample &sample) {
  if (!sdLoggingEnabled) {
    return;
  }

  telemetryLogFile.clearWriteError();
  if (!writeCsvRow(telemetryLogFile, sample) ||
      telemetryLogFile.getWriteError() != 0) {
    disableSdLogging(
        "WARNING: SD log write failed. Disabling SD logging and continuing "
        "serial telemetry.");
    return;
  }

  ++samplesSinceFlush;
  if (samplesSinceFlush < kSdFlushIntervalSamples) {
    return;
  }

  telemetryLogFile.clearWriteError();
  telemetryLogFile.flush();
  if (telemetryLogFile.getWriteError() != 0) {
    disableSdLogging(
        "WARNING: SD log flush failed. Disabling SD logging and continuing "
        "serial telemetry.");
    return;
  }

  samplesSinceFlush = 0;
}

void calibrateGroundPressure() {
  Serial.println("Calibrating ground pressure for 3 seconds. Keep the system stationary.");

  const unsigned long calibrationStartMs = millis();
  float pressureSumPa = 0.0f;
  uint16_t sampleCount = 0;

  while (millis() - calibrationStartMs < kCalibrationDurationMs) {
    pressureSumPa += bme.readPressure();
    ++sampleCount;
    delay(kCalibrationSampleIntervalMs);
  }

  if (sampleCount == 0) {
    haltWithError("ERROR: Ground pressure calibration failed");
  }

  pressureBaselinePa = pressureSumPa / static_cast<float>(sampleCount);

  Serial.print("Ground pressure baseline Pa: ");
  Serial.println(pressureBaselinePa, 2);
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

  configureMpuRanges();
  Serial.println("MPU6050 initialized at +/-16 g and +/-1000 deg/s");

  if (!bme.begin(kBme280Address)) {
    haltWithError("ERROR: Could not find BME280");
  }

  Serial.println("BME280 initialized");
  calibrateGroundPressure();
  initializeSdLogging();
  writeCsvHeader(Serial);
}

void loop() {
  static unsigned long nextSampleTimeMs = 0;
  const unsigned long nowMs = millis();

  if (nextSampleTimeMs == 0) {
    nextSampleTimeMs = nowMs;
  }

  if (static_cast<int32_t>(nowMs - nextSampleTimeMs) < 0) {
    delay(1);
    return;
  }

  nextSampleTimeMs += kLoopPeriodMs;
  if (static_cast<int32_t>(nowMs - nextSampleTimeMs) >= 0) {
    nextSampleTimeMs = nowMs + kLoopPeriodMs;
  }

  const unsigned long time_ms = millis();

  int16_t ax_raw = 0;
  int16_t ay_raw = 0;
  int16_t az_raw = 0;
  int16_t gx_raw = 0;
  int16_t gy_raw = 0;
  int16_t gz_raw = 0;

  mpu.getMotion6(&ax_raw, &ay_raw, &az_raw, &gx_raw, &gy_raw, &gz_raw);

  const float ax_g = ax_raw / kAccelScaleLsbPerG;
  const float ay_g = ay_raw / kAccelScaleLsbPerG;
  const float az_g = az_raw / kAccelScaleLsbPerG;

  const float gx_deg_s = gx_raw / kGyroScaleLsbPerDps;
  const float gy_deg_s = gy_raw / kGyroScaleLsbPerDps;
  const float gz_deg_s = gz_raw / kGyroScaleLsbPerDps;

  const float temp_C = bme.readTemperature();
  const float pressure_Pa = bme.readPressure();
  const float altitudeRelM = computeRelativeAltitudeMeters(pressure_Pa);
  const float altitudeLpfM =
      applyLowPassFilter(altitudeRelM, kAltitudeFilterAlpha, altitudeFilteredM,
                         altitudeFilterInitialized);
  const float totalAccelG = sqrtf(ax_g * ax_g + ay_g * ay_g + az_g * az_g);
  const float totalAccelAverageG = totalAccelAverageFilter.update(totalAccelG);
  const float totalAccelLpfG =
      applyLowPassFilter(totalAccelAverageG, kAccelFilterAlpha,
                         totalAccelFilteredG, totalAccelFilterInitialized);

  const TelemetrySample sample{
      time_ms,        ax_g,              ay_g,       az_g, gx_deg_s,
      gy_deg_s,       gz_deg_s,          temp_C,     pressure_Pa,
      pressureBaselinePa, altitudeRelM, altitudeLpfM, totalAccelG,
      totalAccelLpfG};

  writeCsvRow(Serial, sample);
  logTelemetryToSd(sample);
}
