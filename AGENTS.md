# Repository Guidelines

## Project Structure & Module Organization
- `apps/` contains uploadable sketches; `ProvisioningSetup/` is the fastest demo and `flashlogger/examples/` illustrates the logging pipeline.
- `modules/` hosts reusable components split into `include/`, `src/`, `examples/`, and optional `tests/`. `modules/flash/` ships the mini flash database core, while `modules/sen66/` wraps the particulate sensor.
- `labs/` holds spikes and hardware experiments. Treat anything here as disposable and gate PR merges behind a maintainer review.
- `libraries/` vendors external Arduino dependencies; only bump versions when CI confirms compatibility.
- Keep `library.properties` and `keywords.txt` consistent with any new public types or API entry points.

## Build, Test, and Development Commands
- `arduino-cli compile --fqbn esp32:esp32:esp32c6 apps/ProvisioningSetup/ProvisioningSetup.ino` — sanity-check a shipping sketch for the ESP32-C6 dev kit.
- `arduino-cli upload --port /dev/ttyACM0 --fqbn esp32:esp32:esp32c6 apps/flashlogger/examples/airmonitor_sync.ino` — swap the port for your board path to flash the logger example.
- `g++ -std=c++17 -Imodules/test_support -Imodules/flash/include modules/flash/tests/flash_test.cpp modules/flash/src/FlashStore.cpp -o flash_test && ./flash_test` — run host assertions for the flash store; reuse this recipe for other `tests/` folders.

## Coding Style & Naming Conventions
- Use 2-space indentation, brace-on-same-line, and `CamelCase` class names. Private members trail with `_` as in `Sen66Driver`.
- Group includes as `<Arduino.h>`, then local headers. Prefer `#pragma once` in headers.
- Place all public headers under a module’s `include/` directory and export new symbols via `keywords.txt` to keep Arduino IDE highlighting accurate.

## Testing Guidelines
- Provide host-unit coverage for logic-heavy modules; mirror the `flash_test.cpp` layout and fake Arduino primitives with `modules/test_support/Arduino.h`.
- Name host test executables `<module>_test` and document the exact build command in the module README.
- Hardware-in-the-loop checks belong in `labs/` sketches; document required wiring in comments or the subfolder README.

## Commit & Pull Request Guidelines
- Write short, imperative commit subjects (e.g., `Add SEN66 host stub`) and reference the touched module when helpful, reflecting the existing history.
- PRs must include: a synopsis, target board or lab hardware, verification evidence (command output or screenshots), and linked Jira/GitHub issues when applicable.
- Rebase before requesting review; CI must pass `arduino-cli` compiles for all prod and dev sketches plus host tests you introduce.

## Provisioning & Secrets
- Never commit populated credential files. Instead, copy any `arduino_secrets.h.example` into the sketch’s `include/` directory locally and add it to `.gitignore`.
- Rotate Wi-Fi or API credentials when sharing hardware in the lab, and document secret handoff steps in team channels rather than the repository.
