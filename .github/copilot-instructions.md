# Copilot Instructions

Use `AGENTS.md` and `docs/PROJECT_CONTEXT.md` as the primary project context for this repository.

## Working Summary

- Target: Teensy 4.1 via PlatformIO and Arduino
- Current state: sensor-test prototype, not a finished flight computer
- Active sensors: MPU6050 and BME280 over I2C
- Current behavior: startup pressure calibration, filtered telemetry, serial CSV output

## Required Behavior

- Distinguish clearly between implemented behavior and planned features
- Avoid rescanning the full repo unless the task requires deeper implementation detail
- Prefer incremental edits over speculative rewrites
- Update docs when architecture or behavior changes

## Read Order

1. `AGENTS.md`
2. `README.md`
3. `docs/PROJECT_CONTEXT.md`
4. `src/main.cpp`
