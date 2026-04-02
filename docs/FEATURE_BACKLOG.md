# Feature Backlog

## Near-Term Features

- Move sensor constants and tuning values into a dedicated config header
- Define structs for raw sensor data and computed telemetry
- Extract reusable low-pass and moving-average filters
- Add serial messages for sensor failure and calibration status
- Add basic tests for altitude and filter math

## Mid-Term Features

- Flight-state machine
- Event markers in telemetry output
- Configurable thresholds for launch and apogee detection
- Onboard data logging
- More explicit health and fault reporting

## Long-Term Features

- Deployment output control
- Arming and safety modes
- Redundant sensing or plausibility checks
- Ground-station or radio telemetry
- Replay tools for log-driven debugging
- Hardware-in-the-loop test support

## Nice-To-Have Features

- Compile-time feature flags for bench versus flight builds
- Structured telemetry packets in addition to CSV
- Sensor calibration persistence
- Environmental compensation and more advanced filtering
- Config version stamping in firmware output

## Backlog Maintenance Rule

Keep this list concrete. Remove items that are already implemented, and avoid adding vague entries that cannot be tested or observed.
