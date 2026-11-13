# PalPulse for ESPHome

**Local Powerpal electricity monitoring, live consumption and reliable energy history on ESP32.**

PalPulse is an open-source ESPHome external component that connects a compatible Powerpal electricity meter to an ESP32 over Bluetooth Low Energy (BLE). It makes power, energy usage and battery readings available locally and can forward timestamped measurements to [PalPulse for Home Assistant](https://github.com/iamhasantan/palpulse-homeassistant) for historical energy and cost tracking.

No Powerpal cloud account is required for local metering.

## Features

### Local electricity monitoring

- Direct Powerpal BLE pairing and connection management.
- Live power readings, cumulative energy, daily energy and battery level as ESPHome sensors.
- Configurable meter pairing code, notification interval and pulse calibration.
- Home Assistant integration through ESPHome's native API.

### Reliable history and recovery

- Timestamped native-minute measurement events for accurate historical recording.
- Optional reliable delivery with explicit acknowledgements from Home Assistant.
- Buffered history replay after disconnections, subject to the Powerpal meter retaining the readings.
- Durable per-meter ESP32 NVS checkpoints and independent acknowledged-history cursor.
- Duplicate and out-of-order measurement handling.
- Checkpoint, queue, BLE connection and replay diagnostics for the companion Home Assistant dashboard.

### Local control and integration

- ESP-IDF-based ESPHome external component for supported ESP32 boards.
- Works with the companion PalPulse for Home Assistant integration for hourly statistics, electricity costs and visual recovery controls.
- Local Wi-Fi and BLE operation with ESPHome API encryption and OTA support.
- Open-source MIT-licensed implementation with automated tests and reproducible builds.

## Compatibility

- **Hardware:** A supported ESP32 development board with Bluetooth Low Energy and a compatible Powerpal meter. Select the ESPHome board definition for your actual device.
- **Framework:** ESPHome with ESP-IDF. The repository's validated build baseline is ESPHome 2026.10.2. Other versions and boards require their own verification.
- **History integration:** [PalPulse for Home Assistant](https://github.com/iamhasantan/palpulse-homeassistant), recommended for reliable acknowledgements and native hourly energy history.
- **Firmware:** The example below pins the published ESPHome component version `1.0`. The Home Assistant companion is versioned separately.

## Home Assistant integration requirements

The ESP32 component also works as ordinary ESPHome sensors. For the **full PalPulse for Home Assistant integration**, including historical imports, durable acknowledgements and replay, enable these capabilities:

| Configuration | Purpose |
| --- | --- |
| `api.homeassistant_services: true` | Allows ESPHome to publish timestamped minute measurements and regular checkpoint status to Home Assistant. |
| `api.custom_services: true` | Registers the meter-specific ACK and replay actions used by the companion integration. |
| `sensor.powerpal_ble.history_events: true` | Enables native-minute history events instead of relying only on current energy sensor values. |
| `sensor.powerpal_ble.reliable_history: true` | Holds unacknowledged history records for durable delivery and safe replay. Requires `history_events: true`. |
| `time: platform: homeassistant`, with matching `time_id` | Recommended time synchronisation for reliable timestamp handling. The component supports an optional `time_id`. |
| `esp32_ble_tracker` and `ble_client` | Connect the ESP32 to the intended physical Powerpal meter. |

Also enable **Allow the device to perform Home Assistant actions** for the ESPHome device in Home Assistant. This permission is required for ESPHome to send the history and checkpoint events. **PalPulse for Home Assistant 0.9** automatically discovers the correct per-meter acknowledgement action, so you normally do not need to type an action name.

The `api.encryption` key, Wi-Fi credentials and ESP-IDF board configuration are standard ESPHome settings. They are shown below as part of a complete setup. A static IP address, web server, custom bootloader options and special chip-revision settings are **not required** for PalPulse history integration.

## Complete ESPHome configuration example

This is a complete example for a typical ESP32 development board using ESP-IDF and reliable Powerpal history. It is derived from a working configuration, but removes silicon-revision, memory-placement and flash-chip optimisations that are not portable between devices. Choose the appropriate `board` for your ESP32 and provide your own `secrets.yaml` values.

```yaml
esphome:
  name: powerpal-proxy
  friendly_name: powerpal-proxy

esp32:
  board: esp32dev  # Example board; replace if your ESP32 is different
  framework:
    type: esp-idf

logger:

api:
  encryption:
    key: !secret esphome_api_encryption_key
  reboot_timeout: 0s
  homeassistant_services: true  # Required for HA history/checkpoint events
  custom_services: true          # Required for ACK and replay actions

ota:
  - platform: esphome
    encryption:

wifi:
  ssid: !secret wifi_ssid2
  password: !secret wifi_password2
  fast_connect: true
  min_auth_mode: WPA2
  power_save_mode: none
  reboot_timeout: 0s

external_components:
  - source: github://iamhasantan/palpulse-esphome@1.0
    components: [powerpal_ble]
    refresh: 0s

time:
  - platform: homeassistant
    id: homeassistant_time

esp32_ble_tracker:
  scan_parameters:
    interval: 1100ms
    window: 110ms
    active: false

ble_client:
  - mac_address: !secret powerpal_mac_address
    id: powerpal

sensor:
  - platform: powerpal_ble
    ble_client_id: powerpal
    history_events: true         # Required for timestamped HA history
    reliable_history: true       # Required for acknowledged delivery

    pairing_code: !secret powerpal_pairing_code
    notification_interval: 5
    pulses_per_kwh: !secret powerpal_pulses_per_kwh
    time_id: homeassistant_time

    power:
      name: "Powerpal Power"

    daily_energy:
      name: "Powerpal Daily Energy"

    energy:
      name: "Powerpal Total Energy"

    battery_level:
      name: "Powerpal Battery"
```

### Before flashing

- Add the referenced `!secret` values to your ESPHome `secrets.yaml`. Use the correct BLE MAC, pairing code and **actual meter pulse calibration**. Do not copy another meter's calibration.
- Select the board definition that matches your ESP32. No `minimum_chip_revision`, `sram1_as_iram` or `flash_chip` override is needed for the normal example.
- The Wi-Fi and API encryption options shown are recommended for a local ESPHome deployment. A static IP and optional authenticated `web_server` can be added separately if required.
- The example keeps `reboot_timeout: 0s` for the API and Wi-Fi so a temporary connection loss does not intentionally reboot the device. Adjust these settings for your recovery policy.
- The example retains the encrypted-OTA configuration from the working setup. If you are upgrading older firmware, verify the OTA encryption migration requirements before flashing.
- Preserve NVS and the current working YAML when upgrading. **Do not enable a factory-reset button** to troubleshoot energy gaps or checkpoint mismatches.

### Verify in Home Assistant

1. Confirm the Powerpal BLE client connects and live power, energy and battery readings appear under the ESPHome device.
2. Turn on **Allow the device to perform Home Assistant actions** for that ESPHome device.
3. Add or open **PalPulse for Home Assistant** and select the same physical meter. Automatic history acknowledgements should remain enabled.
4. In **PalPulse → ESP32 recovery**, confirm checkpoint events arrive, the meter-specific ACK action is detected, and the acknowledged checkpoint advances while history is imported.
5. Check Home Assistant's long-term energy statistics before changing your Energy dashboard's consumption source. Do not count both the original ESPHome cumulative-energy sensor and the PalPulse historical statistic as separate grid-consumption sources.

**Important:** Buffered history replay is limited to native-minute readings still stored in the Powerpal meter. ESP32 NVS checkpoints preserve counters and ACK state, not an unlimited archive. The integration does not invent missing electricity measurements or silently replace incompatible history.

## Project status

PalPulse 1.0 packages the existing local metering and reliable-history capabilities for wider use. CI validates the component with ESPHome 2026.10.2 and an ESP-IDF firmware build; different hardware and Home Assistant setups should still be verified after installation. Back up your working ESPHome YAML and preserve the existing NVS checkpoint before firmware changes.

## Links

- [Companion PalPulse for Home Assistant](https://github.com/iamhasantan/palpulse-homeassistant)
- [Technical compatibility guide](docs/advanced.md)
- [Hardware acceptance test plan](docs/TEST_PLAN.md)
- [Version history](CHANGELOG.md)
- [Issues and feedback](https://github.com/iamhasantan/palpulse-esphome/issues)

PalPulse is an independent community project, not affiliated with Powerpal or Home Assistant. Licensed under [MIT](LICENSE). Implementation and research provenance are recorded in [PROVENANCE.md](docs/PROVENANCE.md).
