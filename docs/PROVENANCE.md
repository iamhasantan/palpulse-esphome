# Source provenance and licence boundaries

PalPulse's BLE transport, ESPHome sensor integration and timestamped event
sender are independent project implementations. No C++ transport or ESPHome
code-generation modules from third-party Powerpal integrations were imported.

## Protocol facts and independent implementation

The UUIDs and binary packet formats identify the physical Powerpal BLE
protocol. Reusing these necessary protocol facts does not entail copying a
particular implementation. The original implementation here uses ESPHome and
ESP-IDF's documented interfaces.

Public protocol research by WeekendWarrior1, Pento, Gurrier and muneeb1990
informed earlier experiments. Their names are acknowledged as historical
research contributors, **not** as source-code contributors to PalPulse.

## Earlier original engineering work

The timestamped energy calculations and compatible NVS v1 checkpoint were
originally authored during the repository owner's earlier prototype, then
adapted and refined in PalPulse. They were not copied from the third-party
transport implementations cited above.

The original development history remains available for auditing:
- [Earlier measurement calculation PR #1](https://github.com/iamhasantan/freshpowerpal-esphome/pull/1)
- [Earlier checkpoint PR #2](https://github.com/iamhasantan/freshpowerpal-esphome/pull/2)
- [Earlier checkpoint PR #3](https://github.com/iamhasantan/freshpowerpal-esphome/pull/3)
- [Earlier packet parsing PR #8](https://github.com/iamhasantan/freshpowerpal-esphome/pull/8)

Those links establish provenance for the project's own earlier work and
must not be interpreted as an import of the older repository's inherited BLE
implementation. The new BLE transport was built separately.

## Binary compatibility that cannot be renamed casually

Firmware preserves the existing NVS namespace and `checkpoint` key, v1 40-byte
layout, FPP1 magic, and meter identity hash so that deployed devices can read
their previously saved pulse counts. This is a compatibility contract, not
a project name or current software dependency. Changing it requires a tested
data migration and an explicit operator decision. **Never reset NVS to rebrand
a device.**

## Licence status

Original PalPulse source code is offered under MIT. The linked ESPHome/ESP-IDF
firmware contains third-party dependencies with separate licences and possible
distribution requirements; MIT for this repository does not imply the entire
compiled firmware is MIT-only.

Third-party reuse requires a separate licence review. This provenance
record states implementation history, not a legal opinion.
