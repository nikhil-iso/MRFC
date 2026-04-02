# Roadmap

## Overview

This roadmap is organized from the current prototype state toward a more complete flight computer. It is intentionally conservative and aligned with the code that exists today.

## Phase 0: Sensor Bring-Up

Status: In progress / mostly achieved

Scope:

- Bring up Teensy 4.1 firmware in PlatformIO
- Confirm MPU6050 communication
- Confirm BME280 communication
- Establish serial telemetry output
- Add startup ground-pressure calibration
- Add basic smoothing for noisy signals

Exit criteria:

- Stable boot
- Reliable sensor readings
- Consistent CSV output for bench capture

## Phase 1: Internal Structure

Scope:

- Break `src/main.cpp` into smaller reusable modules
- Move constants into a configuration header
- Separate sensor reads from derived calculations
- Create explicit data structures for raw and filtered telemetry
- Add lightweight test coverage for math and filter logic

Exit criteria:

- Cleaner code organization
- Easier parameter tuning
- Lower risk when adding flight logic

## Phase 2: Flight Logic

Scope:

- Introduce a formal flight-state machine
- Define launch, ascent, coast, apogee, descent, and landed states
- Add event detection thresholds and timing guards
- Record state transitions in telemetry and logs

Exit criteria:

- Flight-state transitions are inspectable and repeatable in test data
- Apogee and landing conditions are explicitly modeled

## Phase 3: Data Logging And Diagnostics

Scope:

- Add onboard logging
- Define a compact and replayable telemetry/log format
- Add boot diagnostics and fault reporting
- Capture configuration values used for each run

Exit criteria:

- Bench and flight sessions can be reconstructed after the fact
- Failures are diagnosable from logs

## Phase 4: Flight Outputs And Safety

Scope:

- Add deployment-channel control
- Introduce arming and inhibit logic
- Add continuity checks and fault handling
- Gate outputs behind explicit state logic and safety conditions

Exit criteria:

- Output behavior is explicit, bounded, and testable
- Safety constraints are documented and enforced in code

## Phase 5: Advanced Reliability

Scope:

- Improve filtering and estimation quality
- Consider sensor redundancy or plausibility checks
- Add watchdog and recovery behaviors
- Expand simulation, replay, and hardware-in-the-loop validation

Exit criteria:

- Firmware is more resilient to sensor noise, bad startup conditions, and runtime faults

## Documentation Rule

When a phase meaningfully changes, update this roadmap and `docs/PROJECT_CONTEXT.md` together.
