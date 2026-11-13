# Changelog

## Unreleased

## 1.0

### Features

- **Local electricity monitoring:** Direct Powerpal BLE pairing, live power, cumulative and daily energy, and battery level through native ESPHome sensor entities.
- **Timestamped history:** Native-minute Powerpal measurements for the companion PalPulse Home Assistant integration.
- **Reliable delivery:** Per-meter ACK actions, bounded buffered replay and an independently persisted acknowledgement cursor.
- **Durable checkpoints:** Per-meter ESP32 NVS counter state, recovery safeguards, duplicate and out-of-order protection.
- **Diagnostics and recovery:** Heartbeat checkpoint events, BLE connection status and safe replay eligibility, including after disconnections.
- **Local control:** ESP-IDF support, ESPHome native API, encrypted connections and OTA support using standard ESPHome configuration.

### Configuration and compatibility

- Provide a complete, hardware-generic working ESPHome YAML example pinned to `github://iamhasantan/palpulse-esphome@1.0`.
- Document the Home Assistant history requirements: `history_events: true`, `reliable_history: true`, `api.homeassistant_services: true`, `api.custom_services: true`, BLE client, and Home Assistant time synchronization.
- Home Assistant users must permit the ESPHome device to perform Home Assistant actions. PalPulse for Home Assistant 0.9 automatically discovers the meter-specific ACK action.
- Preserve existing NVS checkpoints and calibration on upgrade. Do not use factory reset to resolve history gaps.

### Validation

- Unit and sanitizer checks for the existing meter protocol and energy-accounting code.
- ESPHome 2026.10.2 configuration validation and ESP-IDF firmware build using a representative ESP32 configuration.
- Version 1.0 packages the existing 0.2b4 firmware logic without changing BLE, energy accounting, NVS layouts or history-replay algorithms. Real-device compatibility beyond the tested hardware remains dependent on the user's setup.

## 0.2b4

### What's Changed

- **Publish checkpoint heartbeat even without an active Powerpal BLE connection.** The firmware now reports the last saved NVS meter counter and the independent history-ACK cursor roughly once per minute while timestamped history is enabled and the ESPHome native API can send Home Assistant events. This distinguishes BLE outages from failed Home Assistant event delivery.
- **Explicit BLE link health.** Add the `ble_connected` flag to the informational `esphome.palpulse_checkpoint` event. Home Assistant may display the last saved checkpoint while disconnected, but replay remains refused unless the connection and all durability guards pass.
- **No destructive recovery.** The release does not change stored NVS formats, reset the meter, or assume lost minute measurements exist.

### Experimental preview

Requires `history_events: true` and `api.homeassistant_services: true` for event delivery. To enable ACK/replay additionally use `reliable_history: true`, `api.custom_services: true`, the actual per-meter ACK action, and Home Assistant's **Allow the device to perform Home Assistant actions** permission. Back up the current ESPHome YAML and preserve NVS before flashing. Real hardware and BLE reconnect acceptance remains necessary.


## 0.2b3

### What's Changed

- **Sustained acknowledged history catch-up.** Keep reading the Powerpal's buffer while the durable Home Assistant ACK cursor is behind, even after the ESP32's cumulative energy checkpoint has caught up. Avoid an unnecessary five-minute pause after each history replay batch.
- **No extra BLE reads while an ACK queue is still draining.** The firmware keeps the oldest unacknowledged event until Home Assistant confirms a durable write.
- **Recommended reliable setup for new installs.** The documented configuration includes `history_events: true`, `reliable_history: true`, `api.homeassistant_services: true` and `api.custom_services: true`. The CI mirror tests this configuration and no longer advertises a destructive NVS reset button.

### Experimental preview

Retains the existing cumulative NVS v1 checkpoint and independent ACK storage. No NVS migration, reset, or new meter pairing is required. Original history recovery depends on native minute readings still present in the Powerpal buffer. Hardware outage, reconnect and long-soak validation remains necessary. Preserve the original ESPHome YAML and NVS before flashing.


## 0.2b2

### What's Changed

- **ESP32 checkpoint health.** Report the saved Powerpal meter counter, the independently acknowledged history cursor, queue state and replay health to the new PalPulse Home Assistant sidebar.
- **Retry buffered history.** Add a per-meter action to request a BLE re-read from the existing acknowledged cursor. This is a read-only replay request, not a checkpoint reset.
- **Conservative safeguards.** Never clear a replay fault, move the ACK cursor, erase NVS or invent measurements to fill a gap. Recovery is limited to original readings still held by the Powerpal meter.

### Experimental preview

Preserve your ESP32's existing NVS checkpoint and save a copy of the working ESPHome YAML before updating. The sidebar's checkpoint recovery requires **PalPulse for Home Assistant 0.2b5**, `history_events: true`, `reliable_history: true`, and the correct native API ACK action. The existing power, energy, daily energy and battery sensors are unchanged. Hardware behaviour still needs real-world testing.

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
