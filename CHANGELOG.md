# Changelog

## Unreleased

## 0.2b1

### What's Changed

- Add **opt-in acknowledged history** with a native-API action unique to each Powerpal meter. Events remain queued until Home Assistant confirms a durable write.
- Save the acknowledged timestamp and counter separately from the existing NVS v1 energy checkpoint. After restart, unconfirmed records can be replayed from the Powerpal buffer while they remain available.
- Detect missing meter-buffer history or invalid replay-counter continuity and stop safely rather than inventing energy.
- Give additional Powerpal meters their own NVS namespaces. Preserve existing compatible legacy checkpoints without erasing or rewriting them.
- Add the `reliable_history: true` option and compile the matching ESPHome custom-services pathway in CI.
- Refresh the public README and keep the detailed installation notes separately.

### Before testing

- **Experimental firmware.** Back up your ESPHome configuration and current NVS checkpoint. Do not reset NVS to install this build.
- Acknowledged delivery requires PalPulse for Home Assistant **0.2b1**, `api.homeassistant_services: true`, `api.custom_services: true`, and the matching action configured in Home Assistant.
- Legacy best-effort `history_events: true` still works without opting in.
- Replay is limited by the Powerpal meter's own stored-history buffer. Real hardware reconnect and power-loss validation remain necessary.


- Build a separate Powerpal BLE ESPHome component with newly written
  connection, pairing, notification and historical streaming handlers.
- Preserve timestamp-accurate mathematics and native-minute replay developed as original work in an earlier prototype,
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

- Add opt-in `history_events: true` record events carrying Powerpal's original
  UTC timestamp, pulse count and cumulative pulses for the HACS companion.
- Stage timestamped events behind successful NVS commits and pace emission
  through a bounded RAM queue; existing sensor and energy accounting stay unchanged.
