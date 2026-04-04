#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <MPU6050.h>
#include <SD.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {

constexpr uint32_t kSerialBaud = 115200;
constexpr uint32_t kStartupDelayMs = 2000;
constexpr uint32_t kLoopPeriodMs = 50;
constexpr uint32_t kCalibrationDurationMs = 3000;
constexpr uint32_t kCalibrationSampleIntervalMs = 50;
constexpr uint8_t kBuzzerPin = 5;
constexpr uint8_t kBme280Address = 0x77;
constexpr uint8_t kMpuAccelRange = MPU6050_ACCEL_FS_16;
constexpr uint8_t kMpuGyroRange = MPU6050_GYRO_FS_1000;
constexpr float kAltitudeFilterAlpha = 0.2f;
constexpr float kAccelFilterAlpha = 0.2f;
constexpr size_t kAccelAverageWindowSize = 5;
constexpr uint32_t kBuzzerPulseOnMs = 120;
constexpr uint32_t kBuzzerPulseOffMs = 80;
constexpr uint8_t kStartupChirpPulseCount = 2;
constexpr size_t kCommandBufferSize = 96;
constexpr uint16_t kMaxLogFileCount = 1000;
constexpr uint32_t kLogFlushIntervalMs = 1000;

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
bool sdCardReady = false;
bool telemetryEnabled = true;
bool sdLoggingEnabled = false;
char commandBuffer[kCommandBufferSize] = {};
size_t commandLength = 0;
File telemetryLogFile;
char currentLogFilePath[] = "/LOG000.CSV";
unsigned long lastLogFlushMs = 0;

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

void printTelemetryHeader() {
  Serial.println(
      "time_ms,ax_g,ay_g,az_g,gx_deg_s,gy_deg_s,gz_deg_s,temp_C,pressure_Pa,"
      "pressure_baseline_Pa,altitude_rel_m,altitude_lpf_m,a_total_g,"
      "a_total_lpf_g");
}

void writeTelemetryHeader(Print &output) {
  output.println(
      "time_ms,ax_g,ay_g,az_g,gx_deg_s,gy_deg_s,gz_deg_s,temp_C,pressure_Pa,"
      "pressure_baseline_Pa,altitude_rel_m,altitude_lpf_m,a_total_g,"
      "a_total_lpf_g");
}

void writeTelemetryRow(Print &output, unsigned long timeMs, float axG, float ayG,
                       float azG, float gxDegS, float gyDegS, float gzDegS,
                       float tempC, float pressurePa, float pressureBaseline,
                       float altitudeRelM, float altitudeLpfM,
                       float totalAccelG, float totalAccelLpfG) {
  output.print(timeMs);
  output.print(",");
  output.print(axG, 4);
  output.print(",");
  output.print(ayG, 4);
  output.print(",");
  output.print(azG, 4);
  output.print(",");
  output.print(gxDegS, 4);
  output.print(",");
  output.print(gyDegS, 4);
  output.print(",");
  output.print(gzDegS, 4);
  output.print(",");
  output.print(tempC, 2);
  output.print(",");
  output.print(pressurePa, 2);
  output.print(",");
  output.print(pressureBaseline, 2);
  output.print(",");
  output.print(altitudeRelM, 2);
  output.print(",");
  output.print(altitudeLpfM, 2);
  output.print(",");
  output.print(totalAccelG, 4);
  output.print(",");
  output.println(totalAccelLpfG, 4);
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

void configureBuzzer() {
  pinMode(kBuzzerPin, OUTPUT);
  digitalWrite(kBuzzerPin, LOW);
}

void playStartupChirp() {
  for (uint8_t pulse = 0; pulse < kStartupChirpPulseCount; ++pulse) {
    digitalWrite(kBuzzerPin, HIGH);
    delay(kBuzzerPulseOnMs);
    digitalWrite(kBuzzerPin, LOW);

    if (pulse + 1 < kStartupChirpPulseCount) {
      delay(kBuzzerPulseOffMs);
    }
  }
}

void trimWhitespace(char *text) {
  size_t start = 0;
  while (text[start] != '\0' && isspace(static_cast<unsigned char>(text[start]))) {
    ++start;
  }

  if (start > 0) {
    memmove(text, text + start, strlen(text + start) + 1);
  }

  size_t end = strlen(text);
  while (end > 0 &&
         isspace(static_cast<unsigned char>(text[end - 1]))) {
    text[end - 1] = '\0';
    --end;
  }
}

void printCommandHelp() {
  Serial.println("Serial commands:");
  Serial.println("  p               Pause live CSV telemetry");
  Serial.println("  r               Resume live CSV telemetry");
  Serial.println("  telemetry off   Pause live CSV telemetry");
  Serial.println("  telemetry on    Resume live CSV telemetry");
  Serial.println("  sd ls           List files on the onboard SD card");
  Serial.println("  sd cat /FILE    Print a file to serial");
  Serial.println("  sd current      Show the active log file path");
}

void initializeSdCard() {
  sdCardReady = SD.begin(BUILTIN_SDCARD);

  if (sdCardReady) {
    Serial.println("Onboard SD card initialized.");
    Serial.println("Use 'telemetry off' before browsing files if you want a quiet serial output.");
  } else {
    Serial.println("Onboard SD card not detected or failed to initialize.");
  }
}

bool openNextLogFile() {
  if (!sdCardReady) {
    return false;
  }

  for (uint16_t index = 0; index < kMaxLogFileCount; ++index) {
    snprintf(currentLogFilePath, sizeof(currentLogFilePath), "/LOG%03u.CSV",
             static_cast<unsigned int>(index));

    if (SD.exists(currentLogFilePath)) {
      continue;
    }

    telemetryLogFile = SD.open(currentLogFilePath, FILE_WRITE);
    if (!telemetryLogFile) {
      Serial.print("ERROR: Could not create log file: ");
      Serial.println(currentLogFilePath);
      return false;
    }

    writeTelemetryHeader(telemetryLogFile);
    telemetryLogFile.flush();
    lastLogFlushMs = millis();
    sdLoggingEnabled = true;

    Serial.print("Logging CSV to ");
    Serial.println(currentLogFilePath);
    return true;
  }

  Serial.println("ERROR: No available log filename slots remain on the SD card.");
  return false;
}

void flushLogFileIfNeeded(unsigned long nowMs, bool force) {
  if (!sdLoggingEnabled || !telemetryLogFile) {
    return;
  }

  if (force || static_cast<uint32_t>(nowMs - lastLogFlushMs) >= kLogFlushIntervalMs) {
    telemetryLogFile.flush();
    lastLogFlushMs = nowMs;
  }
}

void printCurrentLogFile() {
  if (!sdLoggingEnabled) {
    Serial.println("SD logging is not active.");
    return;
  }

  Serial.print("Active log file: ");
  Serial.println(currentLogFilePath);
}

void listDirectory(File directory, uint8_t depth) {
  while (true) {
    File entry = directory.openNextFile();
    if (!entry) {
      return;
    }

    for (uint8_t level = 0; level < depth; ++level) {
      Serial.print("  ");
    }

    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      listDirectory(entry, depth + 1);
    } else {
      Serial.print(" (");
      Serial.print(entry.size());
      Serial.println(" bytes)");
    }

    entry.close();
  }
}

void listSdCardFiles() {
  if (!sdCardReady) {
    Serial.println("ERROR: SD card is not available.");
    return;
  }

  flushLogFileIfNeeded(millis(), true);

  File root = SD.open("/");
  if (!root || !root.isDirectory()) {
    Serial.println("ERROR: Could not open SD card root directory.");
    return;
  }

  Serial.println("SD card contents:");
  listDirectory(root, 0);
  root.close();
}

void printSdFile(const char *path) {
  if (!sdCardReady) {
    Serial.println("ERROR: SD card is not available.");
    return;
  }

  if (path == nullptr || path[0] == '\0') {
    Serial.println("ERROR: Missing file path. Usage: sd cat /FILE");
    return;
  }

  flushLogFileIfNeeded(millis(), true);

  File file = SD.open(path, FILE_READ);
  if (!file) {
    Serial.print("ERROR: Could not open file: ");
    Serial.println(path);
    return;
  }

  if (file.isDirectory()) {
    Serial.print("ERROR: Path is a directory: ");
    Serial.println(path);
    file.close();
    return;
  }

  Serial.print("BEGIN FILE ");
  Serial.println(path);

  while (file.available()) {
    Serial.write(file.read());
  }

  if (file.size() > 0) {
    Serial.println();
  }

  Serial.print("END FILE ");
  Serial.println(path);
  file.close();
}

void handleCommand(char *command) {
  trimWhitespace(command);
  if (command[0] == '\0') {
    return;
  }

  if (strcmp(command, "help") == 0) {
    printCommandHelp();
    return;
  }

  if (strcmp(command, "p") == 0 || strcmp(command, "telemetry off") == 0) {
    telemetryEnabled = false;
    Serial.println("Telemetry paused.");
    return;
  }

  if (strcmp(command, "r") == 0 || strcmp(command, "telemetry on") == 0) {
    telemetryEnabled = true;
    Serial.println("Telemetry resumed.");
    printTelemetryHeader();
    return;
  }

  if (strncmp(command, "sd ", 3) == 0 && telemetryEnabled) {
    telemetryEnabled = false;
    Serial.println("Telemetry paused for SD card access.");
  }

  if (strcmp(command, "sd ls") == 0) {
    listSdCardFiles();
    return;
  }

  if (strcmp(command, "sd current") == 0) {
    printCurrentLogFile();
    return;
  }

  if (strncmp(command, "sd cat ", 7) == 0) {
    char *path = command + 7;
    trimWhitespace(path);
    printSdFile(path);
    return;
  }

  Serial.print("ERROR: Unknown command: ");
  Serial.println(command);
  printCommandHelp();
}

void handleSerialCommands() {
  while (Serial.available() > 0) {
    const char incoming = static_cast<char>(Serial.read());

    if (incoming == '\r' || incoming == '\n') {
      if (commandLength > 0) {
        commandBuffer[commandLength] = '\0';
        handleCommand(commandBuffer);
        commandLength = 0;
      }
      continue;
    }

    if (telemetryEnabled && commandLength == 0) {
      telemetryEnabled = false;
      Serial.println("Telemetry paused for command entry. Type 'r' or 'telemetry on' to resume.");
    }

    if (incoming == '\b' || incoming == 127) {
      if (commandLength > 0) {
        --commandLength;
      }
      continue;
    }

    if (commandLength + 1 >= kCommandBufferSize) {
      commandLength = 0;
      Serial.println("ERROR: Command too long.");
      continue;
    }

    commandBuffer[commandLength++] = incoming;
  }
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
  configureBuzzer();
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
  playStartupChirp();
  initializeSdCard();
  openNextLogFile();
  printCommandHelp();
  printTelemetryHeader();
}

void loop() {
  handleSerialCommands();

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

  if (telemetryEnabled) {
    writeTelemetryRow(Serial, time_ms, ax_g, ay_g, az_g, gx_deg_s, gy_deg_s,
                      gz_deg_s, temp_C, pressure_Pa, pressureBaselinePa,
                      altitudeRelM, altitudeLpfM, totalAccelG, totalAccelLpfG);
  }

  if (sdLoggingEnabled && telemetryLogFile) {
    writeTelemetryRow(telemetryLogFile, time_ms, ax_g, ay_g, az_g, gx_deg_s,
                      gy_deg_s, gz_deg_s, temp_C, pressure_Pa,
                      pressureBaselinePa, altitudeRelM, altitudeLpfM,
                      totalAccelG, totalAccelLpfG);
    flushLogFileIfNeeded(nowMs, false);
  }
}
