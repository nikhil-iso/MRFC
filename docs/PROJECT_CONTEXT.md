# Project Context

## What This Repository Is

This repository contains firmware for MRFC, a Teensy 4.1 based rocket flight computer project. At the current stage, the repository is centered on proving out sensor reads, startup calibration, basic filtering, a simple startup buzzer indicator, and serial telemetry.

The repo also includes a lightweight browser-based bench dashboard under `dashboard/` that consumes the existing serial CSV stream for MPU6050 visualization.

This is a prototype foundation, not a complete avionics stack.

## What The Firmware Does Today

The current firmware lives entirely in `src/main.cpp` and performs the following sequence:

1. Starts serial output at `115200`
2. Waits `2 seconds` after boot
3. Starts I2C at `400 kHz`
4. Initializes the MPU6050 and verifies connection
5. Sets MPU6050 ranges to `+/-16 g` and `+/-1000 deg/s`
6. Initializes the BME280 at address `0x77`
7. Calibrates baseline pressure over `3 seconds` while stationary
8. Emits a short active-buzzer chirp on pin `5` to confirm successful startup
9. Enters a nominal `50 ms` loop
10. Reads IMU and barometric data
11. Computes relative altitude from pressure
12. Filters altitude and total acceleration
13. Emits CSV telemetry over serial

## Derived Signals

The firmware currently derives:

- Relative altitude in meters from live pressure versus the startup pressure baseline
- Low-pass filtered relative altitude
- Total acceleration magnitude in g
- Moving-average plus low-pass filtered total acceleration

Filtering is intentionally simple and suitable for early bring-up, not final flight-quality state estimation.

## Architectural Reality

The current code structure is intentionally minimal:

- One source file
- Global sensor instances
- File-local constants and helper functions
- No classes or modules for sensor adapters, filters, or flight states

This keeps the prototype easy to inspect, but it will become limiting as features grow.

## Bench Dashboard

The optional host-side dashboard currently:

- Runs locally in a browser via Web Serial
- Connects directly to the Teensy CSV stream at `115200`
- Displays live accelerometer and gyroscope values
- Draws rolling charts for IMU axes
- Renders a simple relative orientation view using a complementary filter

The orientation display is a bench aid only:

- Pitch and roll are stabilized using accelerometer tilt
- Yaw is relative from connect or reset and will drift over time
- It is not flight-grade attitude estimation

## Constraints And Risks

Current known limitations:

- No explicit flight-state machine
- No apogee detection logic
- No deployment outputs or safety interlocks
- Startup buzzer is only a post-init readiness indicator and not tied to runtime events
- No SD logging or non-volatile event storage
- No watchdog or fault-recovery behavior
- No redundancy or cross-checking between sensors
- No tests
- No hardware abstraction layer
- Pressure baseline is assumed valid after a stationary 3 second startup
- Altitude estimate is sensitive to barometric noise and environmental drift

## Likely Near-Term Refactor Direction

As the project matures, the next useful structure is likely:

- `src/main.cpp` as thin orchestration only
- `include/` and `lib/` for reusable modules
- sensor interface layer
- filter and derived-signal layer
- flight-state logic layer
- output/logging layer
- configuration constants collected into one place

This is a suggested direction, not a completed design.

## Definition Of "Current Truth"

When documentation and code differ, prefer:

1. Code for current implemented behavior
2. `docs/ROADMAP.md` for planned behavior
3. `docs/GOALS.md` for intent and priorities

If a code change alters the behavior above, update this file in the same patch.
