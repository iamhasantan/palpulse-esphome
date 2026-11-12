# PalPulse for ESPHome

A local Bluetooth connection between a **Powerpal electricity meter** and an ESP32. The component reports live power, energy totals, daily usage and battery level to ESPHome. It can also send timestamped meter readings to [PalPulse for Home Assistant](https://github.com/iamhasantan/palpulse-homeassistant).

> **0.2b2 is an early preview.** We're testing reconnects, buffered history, power loss and upgrades. **Back up your working ESPHome configuration and preserve the NVS checkpoint before flashing.** Do not use factory reset to fix an update.

## Getting started

Use an ESP32 with ESP-IDF and ESPHome 2026.10.2 or newer. Add the component to your existing ESPHome YAML:

```yaml
external_components:
  - source: github://iamhasantan/palpulse-esphome@0.2b2
    components: [powerpal_ble]

esp32_ble_tracker:

ble_client:
  - id: powerpal
    mac_address: !secret powerpal_mac_address

time:
  - platform: homeassistant
    id: ha_time

api:
  homeassistant_services: true

sensor:
  - platform: powerpal_ble
    ble_client_id: powerpal
    pairing_code: !secret powerpal_pairing_code
    notification_interval: 5
    pulses_per_kwh: 3200 # use YOUR meter's actual calibration
    time_id: ha_time
    history_events: true
    power:
      name: "Powerpal Power"
    energy:
      name: "Powerpal Total Energy"
    daily_energy:
      name: "Powerpal Daily Energy"
    battery_level:
      name: "Powerpal Battery"
```

Add these to your **existing** ESPHome blocks. Don't duplicate `api:` or `sensor:`. The example leaves out common Wi-Fi, ESP32, encryption and OTA settings, which you'll need to configure separately.

## Preview: acknowledged history

The optional acknowledged mode keeps unconfirmed records until Home Assistant accepts them. Use it with **PalPulse for Home Assistant 0.2b5** or a compatible newer preview:

```yaml
api:
  homeassistant_services: true
  custom_services: true

sensor:
  - platform: powerpal_ble
    history_events: true
    reliable_history: true
```

Keep the rest of your existing settings. Under Home Assistant's ESPHome device options, allow the device to perform Home Assistant actions. In the PalPulse integration's **History delivery and acknowledgments** settings, enter your device's action, for example `esphome.powerpal_proxy_palpulse_ack_1a2b3c4d`. Each meter registers its own action with an eight-character meter hash suffix. Confirm the **actual** action name in Home Assistant, and use that rather than the example.

This is **not an unlimited archive**. Recovery still depends on readings remaining in the Powerpal's own buffer. If the meter has discarded them, PalPulse refuses to invent history. The old checkpoint is kept compatible with existing firmware.

## ESP32 checkpoint recovery in the PalPulse dashboard

With **ESPHome 0.2b2** and the corresponding Home Assistant preview, the
firmware reports its saved NVS meter counter, the independently acknowledged
history cursor and current queue/replay health. The Home Assistant **PalPulse**
sidebar displays those values separately from Recorder statistics.

If you enable `reliable_history: true` and the corresponding ACK action, the
panel offers **Retry buffered history from ESP checkpoint**. This requests a
normal BLE re-read from the last acknowledged cursor. It **does not reset NVS,
move the saved counter or force Home Assistant to accept incomplete hours**.
The action will refuse unsafe situations including replay faults, missing
ACK setup and checkpoints that cannot be confirmed saved.

The matching native action has the same device prefix and meter hash as your
ACK action, but uses `palpulse_replay_` instead of `palpulse_ack_`. Check
the actual actions listed under your ESPHome device. Nothing needs to be
entered manually for the replay action in the PalPulse dashboard.

The firmware reports checkpoints periodically when connected and history
events are enabled. **Updating ESPHome does not automatically restore old
missing hours**. Recovery only works while the Powerpal meter still has
those original records.

## Notes

- Use one active BLE connection to a Powerpal at a time.
- Keep the original device name and sensor names when upgrading if you want to preserve existing Home Assistant entity IDs.
- Each new meter gets its own NVS namespace. Existing compatible legacy checkpoints remain unchanged.
- Never reset NVS or change the meter's pulse calibration to make history line up.

[Technical and compatibility guide](docs/advanced.md) · [Test plan](docs/TEST_PLAN.md) · [Changelog](CHANGELOG.md) · [Report an issue](https://github.com/iamhasantan/palpulse-esphome/issues)

An independent community project, not affiliated with Powerpal or Home Assistant. [MIT source license](LICENSE). Protocol research and implementation provenance are credited in [PROVENANCE.md](docs/PROVENANCE.md).
