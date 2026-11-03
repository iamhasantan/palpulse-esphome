# Hardware acceptance plan

## Automated requirements

- C++17 helpers compile with warnings as errors.
- BLE payload tests pass, including truncated and null inputs.
- C++ sanitizers pass.
- ESPHome 2026.10.2 validates the example config and compiles full ESP-IDF
  firmware for classic ESP32 within an 18-minute timeout.

## Hardware validation before merge

Run on a **spare ESP32**, keeping production FreshPowerpal firmware operational.
Record the commit SHA, YAML, raw Powerpal BLE logs, initial total pulses, and
Home Assistant raw sensor states. Preserve NVS and back up configuration.

1. Pair with a real Powerpal, inspect characteristic discovery and write ACKs.
2. Verify live watts against historical 60-second power and a known load.
3. Observe at least six consecutive five-minute batches with correct totals.
4. Disconnect BLE, allow buffered accumulation, reconnect, and compare pulse
   deltas with exact expected record timestamps.
5. Reboot after a checkpoint and verify exact total pulse continuity.
6. Test missing and incompatible NVS checkpoints on a *test* device.
7. Test local midnight and catch-up after the day boundary.
8. Confirm entity IDs, total-increasing statistics and battery reporting.
9. Check logs contain no pairing codes or cloud API credentials.
10. Do not alter, backfill or erase Home Assistant long-term statistics.

A passing CI build is **not** a release or hardware safety approval. Obtain
explicit owner approval before merging or replacing production firmware.

## HACS companion acceptance tests

- Enable `api.homeassistant_services: true` plus `history_events: true` and
  allow Home Assistant actions for the ESPHome device.
- Verify `esphome.palpulse_record` events contain UTC timestamps, exact pulses,
  cumulative pulse counts, the meter MAC and calibration. No credentials.
- Test one complete UTC hour with 60 native-minute records, then confirm the
  external statistic `palpulse:powerpal_<mac>_energy` has the correct hour.
- Shut down Home Assistant, allow buffered Powerpal consumption, restart, and
  verify missing records **block** statistics imports instead of causing a spike.
- Exercise device disconnect, BLE reconnect, reboot and repeated record streams.
  Ensure old sensor statistics are not simultaneously selected in Energy.
