# AGENTS.md

This file is the fast-start context for Codex and other AI coding tools working in this repository. Use it before scanning the repo in depth.

## Project Summary

MRFC is currently a Teensy 4.1 firmware prototype for a rocket flight computer. The present implementation is a sensor-test and telemetry stage, not a production flight stack.

The code today:

- Reads an MPU6050 and BME280 over I2C
- Calibrates baseline pressure during startup
- Computes relative altitude from pressure
- Applies simple smoothing to altitude and total acceleration
- Streams sensor and derived values as CSV over serial

The code does not yet:

- Detect flight phases
- Control deployment channels
- Log to onboard storage
- Transmit telemetry wirelessly
- Separate hardware drivers, filtering, and flight logic into modules

## Canonical Files

Prefer these files as the primary context sources:

1. `README.md`
2. `docs/PROJECT_CONTEXT.md`
3. `docs/GOALS.md`
4. `docs/ROADMAP.md`
5. `docs/MILESTONES.md`
6. `src/main.cpp`

Only scan beyond those files if the task requires code-level detail not already captured there.

## Current Technical Baseline

- Board: Teensy 4.1
- Framework: Arduino
- Build system: PlatformIO
- Main environment: `env:teensy41`
- Libraries:
  - `electroniccats/MPU6050`
  - `adafruit/Adafruit BME280 Library`
- Main file: `src/main.cpp`

Operational constants currently in code:

- Serial baud: `115200`
- Startup delay: `2000 ms`
- Sampling period: `50 ms`
- Pressure calibration duration: `3000 ms`
- Pressure calibration interval: `50 ms`
- BME280 I2C address: `0x77`
- MPU6050 accel range: `+/-16 g`
- MPU6050 gyro range: `+/-1000 deg/s`

## Working Assumptions

- Treat the current firmware as a prototype baseline, not the final architecture.
- Keep documentation honest about what exists today versus what is planned.
- Do not invent hardware interfaces, deployment outputs, or sensor redundancy that is not present in code or docs.
- Prefer incremental refactors over large speculative rewrites.
- If adding new subsystems, update the docs in the same change set.

## Preferred AI Workflow

For most tasks:

1. Read this file and `docs/PROJECT_CONTEXT.md`
2. Read `src/main.cpp`
3. Check whether the request is current-state work or future-state planning
4. Implement the smallest coherent change
5. Update relevant docs if behavior, scope, or structure changes

## Session Bootstrap Prompt

Use this prompt when handing the project to another AI tool:

```text
Read AGENTS.md and docs/PROJECT_CONTEXT.md first. Treat them as the primary project context unless the code clearly disagrees. Avoid rescanning the whole repository unless the task requires deeper inspection. Keep documentation aligned with the current implementation and distinguish clearly between existing behavior and planned features.
```
