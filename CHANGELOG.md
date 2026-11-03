# Changelog

## Unreleased

- Build a separate Powerpal BLE ESPHome component with newly written
  connection, pairing, notification and historical streaming handlers.
- Adapt original FreshPowerpal timestamp mathematics, native-minute replay,
  NVS v1 checkpoints and defensive packet decoding, with explicit credits.
- Preserve existing ESPHome sensor options, entity naming and checkpoint format
  where practical; protect incompatible NVS data from being overwritten.
- Add packet/energy regression tests, sanitizer checks, a full ESP-IDF firmware
  build and a hardware validation plan.

**Status:** development. No hardware validation or production release yet.
