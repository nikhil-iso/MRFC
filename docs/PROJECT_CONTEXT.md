# Project Context

## What This Repository Is

This repository contains firmware for MRFC, a Teensy 4.1 based rocket flight computer project. At the current stage, the repository is centered on proving out sensor reads, startup calibration, basic filtering, serial telemetry, and onboard CSV session logging.

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
8. Enters a nominal `50 ms` loop
9. Reads IMU and barometric data
10. Computes relative altitude from pressure
11. Filters altitude and total acceleration
12. Emits CSV telemetry over serial
13. Writes the same CSV rows to an onboard SD card log file when available

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

## Constraints And Risks

Current known limitations:

- No explicit flight-state machine
- No apogee detection logic
- No deployment outputs or safety interlocks
- No structured event/fault logging beyond raw SD CSV session capture
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
