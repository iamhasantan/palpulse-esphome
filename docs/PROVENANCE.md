# Code provenance and licensing

PalPulse's BLE transport and ESPHome component integration are new code written
for this repository. They do not copy the C++ transport implementation of
WeekendWarrior1, Pento or Gurrier.

The Powerpal BLE UUIDs and packet layouts describe a device protocol. Those
facts were checked against hardware observations recorded during the
FreshPowerpal project, and independently available technical references.
Protocol compatibility necessarily uses those same UUIDs and numeric encodings.

## Reused original FreshPowerpal work

The helper algorithms in `accumulator.h` are adapted from the newly developed
FreshPowerpal `measurement_math.h` (PR #1), `energy_checkpoint.h` (PRs #2–3).
They preserve the original FreshPowerpal checkpoint layout, key derivation and
measurement semantics for compatibility. Packet decoding concepts in `wire.h`
are informed by FreshPowerpal's original `ble_packet.h` (PR #8), but use a new
interface and implementation.

- [FreshPowerpal PR #1](https://github.com/iamhasantan/freshpowerpal-esphome/pull/1)
- [FreshPowerpal PR #2](https://github.com/iamhasantan/freshpowerpal-esphome/pull/2)
- [FreshPowerpal PR #3](https://github.com/iamhasantan/freshpowerpal-esphome/pull/3)
- [FreshPowerpal PR #8](https://github.com/iamhasantan/freshpowerpal-esphome/pull/8)

The FreshPowerpal repository has an MIT LICENSE, but its *inherited* portions
from Pento/WeekendWarrior1 have unresolved historical GPL provenance. For this
reason the inherited transport and code-generation implementation have **not**
been imported.

## Historical research attribution

- WeekendWarrior1: original Powerpal ESPHome work and protocol discoveries.
- Pento/Gary Pendergast: standalone external-component implementation and
  subsequent protocol and compatibility research.
- Gurrier and muneeb1990: other prior Powerpal BLE implementations and research.

In particular, Gurrier's implementation is GPLv3. No Gurrier source code is
included in PalPulse.

## Licence distinction

The original source authored for PalPulse is offered under MIT. This does not
claim that a *compiled ESPHome firmware image* is MIT-only: ESPHome's own C++
runtime and linked dependencies have their own licences and distribution
requirements. Reusing the old FreshPowerpal v1 checkpoint is a data-format
compatibility choice, not an attempt to relicense inherited transport code.

This is an engineering provenance record, not a legal opinion. Any future
third-party source import requires an explicit licence and provenance review.
