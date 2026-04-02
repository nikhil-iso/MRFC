# Goals

## Project Goals

The project appears to be aiming toward a practical rocket flight computer firmware base on Teensy 4.1. The current implementation suggests the following goals.

## Primary Goals

- Build a reliable embedded baseline for flight sensing on Teensy 4.1
- Validate barometric and inertial data quality early
- Produce telemetry that is easy to log, inspect, and tune
- Evolve from sensor prototype to a flight-state-aware avionics stack
- Keep the code understandable enough for rapid iteration with AI-assisted development

## Technical Goals

- Maintain deterministic sampling behavior
- Improve confidence in altitude and acceleration estimates
- Separate sensor access, filtering, and decision logic into maintainable modules
- Add safe event logic for rocket flight phases
- Support repeatable testing and regression checks

## Operational Goals

- Make startup behavior predictable and observable
- Detect sensor initialization failures clearly
- Preserve enough data to analyze flights and bench tests
- Enable future integration with deployment hardware and telemetry links

## Documentation Goals

- Keep a stable project summary that AI tools can trust
- Distinguish clearly between implemented and planned behavior
- Reduce repeated repo rediscovery in future sessions
- Capture roadmap and milestone intent in plain language

## Non-Goals For The Current Stage

These are not realistic assumptions for the repo today:

- Full flight readiness
- Redundant avionics
- Advanced sensor fusion
- Mission management UI
- Production-grade fault tolerance

Those may become later goals, but they are not implemented now.
