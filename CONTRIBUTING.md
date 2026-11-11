# Contributing

Create focused development branches and changelog-friendly pull requests.
Keep `main` and production firmware unchanged until the owner approves.

Do not import third-party Powerpal BLE implementations without an explicit
licence and provenance review. Our own earlier measurement and checkpoint
helpers may be reused with their original authorship recorded. Maintain the
source provenance in `docs/PROVENANCE.md` rather than erasing audit history.

Never erase NVS, modify Home Assistant statistics, log cloud API credentials,
or deploy a firmware image automatically from CI.
