# MRFC

MRFC is a PlatformIO firmware project targeting a Teensy 4.1. The current codebase is an early flight-computer sensor prototype that reads an MPU6050 IMU and a BME280 barometric sensor, calibrates a ground-pressure baseline at startup, emits a short buzzer-ready chirp on successful initialization, and streams filtered telemetry over serial as CSV.

## Current Status

The project is in a sensor-validation phase. It is not yet a complete flight computer. The firmware currently focuses on:

- Initializing the MPU6050 and BME280 on I2C
- Calibrating baseline pressure over a 3 second stationary startup window
- Driving an active startup buzzer on Teensy pin `5` after initialization succeeds
- Computing relative altitude from live pressure
- Smoothing altitude and total acceleration with simple filters
- Emitting a CSV telemetry stream for logging and tuning

Not yet implemented:

- Flight-state detection
- Event handling for launch, burnout, apogee, and landing
- Pyro channel control
- Persistent data logging
- Radio or ground-station link
- Sensor abstractions, unit tests, or a multi-module architecture

## Hardware And Firmware Baseline

- MCU: Teensy 4.1
- Framework: Arduino via PlatformIO
- Sensors:
  - MPU6050 IMU
  - BME280 barometric sensor
- Bus: I2C via `Wire`
- Buzzer: active buzzer signal on Teensy pin `5`
- Serial baud: `115200`
- Loop period: `50 ms` nominal
- BME280 address: `0x77`

## Telemetry Output

The firmware prints CSV with the following columns:

- `time_ms`
- `ax_g`, `ay_g`, `az_g`
- `gx_deg_s`, `gy_deg_s`, `gz_deg_s`
- `temp_C`
- `pressure_Pa`
- `pressure_baseline_Pa`
- `altitude_rel_m`
- `altitude_lpf_m`
- `a_total_g`
- `a_total_lpf_g`

## Repo Layout

- `src/main.cpp`: Current firmware implementation
- `platformio.ini`: Build target and library dependencies
- `include/`: Reserved for shared headers
- `lib/`: Reserved for project-specific libraries
- `test/`: Reserved for PlatformIO tests
- `docs/`: Project context, goals, roadmap, and AI handoff material
- `AGENTS.md`: Fast-start context for Codex and other AI tools

## Build And Monitor

```powershell
pio run
pio device monitor -b 115200
```

## Recommended Reading Order For Future Work

When starting a new session with Codex or another AI tool, read these first:

1. `AGENTS.md`
2. `docs/PROJECT_CONTEXT.md`
3. `docs/ROADMAP.md`
4. `src/main.cpp`

That order gives enough context for most tasks without rescanning the full repository.
