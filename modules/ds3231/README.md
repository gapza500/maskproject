# DS3231 Real-Time Clock Helper

`Ds3231Clock` wraps Adafruit's RTClib `RTC_DS3231` driver so the rest of the
firmware can share a single DS3231 instance. The wrapper wires together the
canonical `begin(TwoWire&)`, `lostPower()`, `running()`, `adjust()`,
`now()`, and `getTemperature()` calls while keeping the API compact for
consumer code.

## Layout

- `include/Ds3231Clock.h` — wrapper header exposing the shared API.
- `src/Ds3231Clock.cpp` — RTClib-backed implementation.
- `examples/` — simple usage sketch reading time + temperature.
- `checkers/` — serial diagnostic sketch for hardware bring-up.

> Host-side unit tests are currently disabled because RTClib depends on the
> Arduino `TwoWire` implementation. Run the checker sketch on hardware to
> validate wiring.

## Usage Notes

1. Call `Wire.begin()` before `Ds3231Clock::begin()` (the helper does this in
   the examples for convenience).
2. Whenever Wi-Fi connectivity is available, call `adjust()` with the latest
   network time to keep the clock in sync (this will be handled in the main
   control app).
3. The device-status provider (`labs/devicestatus_lib`) consumes the same wrapper,
   so any updates to the clock immediately flow through the status report
   available to the UI/dashboard.
