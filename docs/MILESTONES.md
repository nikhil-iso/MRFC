# Milestones

## Milestone 1: Bench Telemetry Baseline

Objective:
Produce stable CSV telemetry from the Teensy 4.1 with working MPU6050 and BME280 reads.

Success indicators:

- Clean boot
- Sensor initialization succeeds consistently
- CSV stream includes raw and derived fields
- Pressure baseline calibration completes without error

Current status:
Essentially complete based on the current code.

## Milestone 2: Refactor Into Modules

Objective:
Split the prototype into maintainable pieces before adding more behavior.

Success indicators:

- Sensor code moved out of `src/main.cpp`
- Filter logic isolated and reusable
- Constants gathered in one location
- Documentation updated to match the new layout

## Milestone 3: Flight-State Prototype

Objective:
Add first-pass logic for launch, coast, apogee, descent, and landed detection.

Success indicators:

- State machine exists in code
- Transition criteria are documented
- State changes are observable in serial/log output
- Bench and replay tests cover expected transitions

## Milestone 4: Persistent Logging

Objective:
Retain flight and bench data beyond the live serial stream.

Success indicators:

- Onboard logging enabled
- Log format documented
- Session metadata and fault events captured

Current status:
Started. The firmware now creates sequential boot-session CSV logs on the onboard SD card, but richer metadata and event capture are still pending.

## Milestone 5: Controlled Outputs

Objective:
Integrate deployment or other flight outputs safely.

Success indicators:

- Output channels abstracted in code
- Safety checks documented and enforced
- State-driven activation logic implemented
- Dry-run and inhibited modes available for testing

## Milestone 6: Validation And Hardening

Objective:
Move from prototype behavior toward a more trustworthy avionics stack.

Success indicators:

- Wider test coverage
- Better fault handling
- Replay or simulation support
- Clearer operational procedures and limits
