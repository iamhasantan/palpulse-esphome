# PalPulse for ESPHome

An independent **Powerpal BLE** external component for ESPHome and Home Assistant.
Runs locally on ESP32 with **ESP-IDF**. No Powerpal cloud dependency.

**Status: experimental.** Source is in a draft PR; the component has not been
validated on Powerpal hardware. Do not replace production firmware until hardware
and Home Assistant statistics checks have passed.

## Features

- Direct Powerpal BLE pairing, characteristic discovery and notifications
- Timestamp-correct watts, native-minute historical replay and live pulse watts
- Versioned, per-meter NVS checkpoint compatible with FreshPowerpal v1
- Duplicate/out-of-order rejection, bounded catch-up and conservative daily totals
- Cumulative energy, daily energy, battery and watts as ordinary ESPHome sensors
- C++ unit tests, sanitizers and an ESPHome 2026.10.2 ESP-IDF build workflow

## Configuration

Keep the same `esphome.name` and the same sensor names as the existing device
to preserve Home Assistant entity IDs. Do **not** run two BLE bridges against
the meter simultaneously.

```yaml
external_components:
  - source: github://iamhasantan/palpulse-esphome@feat/independent-powerpal-ble-foundation
    components: [powerpal_ble]
    refresh: 0s

esp32_ble_tracker:

ble_client:
  - id: powerpal_client
    mac_address: !secret powerpal_mac_address

time:
  - platform: homeassistant
    id: ha_time

sensor:
  - platform: powerpal_ble
    ble_client_id: powerpal_client
    pairing_code: !secret powerpal_pairing_code
    notification_interval: 5
    pulses_per_kwh: 3200  # replace with your meter's calibration
    time_id: ha_time
    live_power: true
    power:
      name: "Powerpal Power"
    energy:
      name: "Powerpal Total Energy"
    daily_energy:
      name: "Powerpal Daily Energy"
    battery_level:
      name: "Powerpal Battery"
```

This fragment requires the normal `esphome:`, `esp32:`, `wifi:`,
`logger:` and `api:` blocks. Set `esp32.framework.type: esp-idf`.
`ci-powerpal.yaml` has an example using **fake** credentials and device
values. It is exclusively for validation, not direct installation.

Replace the branch ref with a reviewed commit SHA when testing firmware.

## HACS companion: timestamped historical energy

The optional [PalPulse Home Assistant](https://github.com/iamhasantan/palpulse-homeassistant)
HACS integration can import complete native-minute record sequences as correctly
timestamped, hourly **external** energy statistics. It does not overwrite the
ordinary Total Energy sensor or Home Assistant recorder history.

To enable the experimental data path, add to the **existing** YAML blocks:

```yaml
api:
  homeassistant_services: true  # retain your existing encryption and reboot options

sensor:
  - platform: powerpal_ble
    history_events: true        # retain all your existing sensor options
```

Within the Home Assistant ESPHome device's configuration, enable
**Allow the device to perform Home Assistant actions** or events will not arrive.
The event name is `esphome.palpulse_record`. Each event contains
`meter`, original `timestamp`, `pulses`, `total_pulses`, and
`pulses_per_kwh`. No pairing code or cloud API key is transmitted.

The sender is **opt-in** and uses a bounded 512-record RAM queue, paced at about
8 events/second. It emits only after NVS checkpoint persistence succeeds.
These native API events are **not acknowledged**. Events can still be lost if
Home Assistant is disconnected or its queue cannot keep up; the HACS
importer refuses incomplete hours instead of inventing consumption. That makes
this beta deliberately conservative, not a lossless history transport.

Do **not** select both existing Powerpal Total Energy and the new external
statistic as two grid consumption sources. Test the new external statistic
independently before switching your Energy dashboard.

## Compatibility

Existing `powerpal_ble` YAML configuration keys remain supported:
`pairing_code`, `pulses_per_kwh`, `notification_interval`, `time_id`,
`live_power`, `power`, `energy`, `daily_energy`, `battery_level`.
The old `log_api_key: true` option is intentionally rejected: cloud
API credentials must never be printed by the component.

### Legacy NVS checkpoint

PalPulse uses the FreshPowerpal NVS namespace `freshpowerpal`, key
`checkpoint` and v1 40-byte FPP1 payload. It validates meter identity,
calibration, magic and schema version. If a checkpoint is incompatible, it
leaves the existing NVS data untouched and **withholds updated total energy**.

On empty NVS, only the most recent meter batch is used as the initial baseline,
not an invented lifetime offset. Do not erase NVS during an upgrade or test.

Total energy is only published after the corresponding cursor and pulse count
have been durably checkpointed. Daily energy is withheld during historical
catch-up and when a trustworthy local day is unavailable. Home Assistant's
historical statistics database is never directly modified.

### Current limitations

Hardware connection timing, clock setting after power loss, battery updates,
midnight rollover and reconnect/replay need on-device testing. Failed or
timed-out stream requests are retried. At most 30 native-minute records are
requested per stream. Advanced diagnostics, cloud-history import and other
forks' extra sensor types are intentionally out of scope.

## Build and test

```sh
make test
python -m pip install "esphome==2026.10.2"
esphome config ci-powerpal.yaml
esphome compile ci-powerpal.yaml
```

See [test plan](docs/TEST_PLAN.md), [source provenance](docs/PROVENANCE.md)
and [changelog](CHANGELOG.md).

## Credit and licence

PalPulse credits earlier Powerpal protocol research by WeekendWarrior1,
Gary Pendergast/Pento, Gurrier and muneeb1990 without importing their GPL-
questionable transport implementations. FreshPowerpal's **new, original**
measurement and checkpoint helpers are adapted with attribution.

Source is offered under [MIT](LICENSE). Compiled ESPHome firmware also contains
third-party runtime dependencies and may have additional GPL obligations.
