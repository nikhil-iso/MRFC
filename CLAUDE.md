# CLAUDE.md

Start with `AGENTS.md` and `docs/PROJECT_CONTEXT.md`. Treat them as the canonical high-level context for this repo unless the code clearly conflicts.

## Project Snapshot

- Repository: MRFC
- Domain: Teensy 4.1 flight-computer firmware prototype
- Current scope: sensor bring-up, pressure calibration, simple filtering, serial CSV telemetry
- Current implementation: mostly contained in `src/main.cpp`

## Expectations

- Do not assume flight-state logic, logging, pyro control, or radio features already exist
- Keep changes aligned with the current prototype maturity
- Prefer small, testable, well-documented steps
- Update the context docs when major changes are made
