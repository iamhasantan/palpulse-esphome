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

- Guard daily-energy publication when local time is unavailable, refresh battery after each completed stream, and mark disconnected watts unavailable.
- Avoid duplicate push and PR CI runs with PR-only workflows and cancellation of superseded runs.

- Fix the 5-minute measurement interval characteristic write to use the
  required four-byte little-endian payload; cover it in the packet tests.
- Handle both BLE disconnect and close events, including missing event data.
- Add a second ESPHome configuration fixture mirroring the user's existing
  powerpal-proxy YAML with non-secret sample values.

**Status:** development. No hardware validation or production release yet.
