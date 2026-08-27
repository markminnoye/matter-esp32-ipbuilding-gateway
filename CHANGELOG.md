# Changelog

All notable changes to the embedded IPBuilding Gateway firmware are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

Firmware version is `PROJECT_VER` in the root `CMakeLists.txt` (Matter software version).

## [Unreleased]

## [0.1.2-e3] - 2026-08-27

### Changed

- **IP0200PoE:** relay status decode uses `00xx`/`01xx` prefix rule — may improve startup status on older field modules (lab `0000`/`0100` unchanged).
- **IP1100PoE / IP0200PoE / IP0300PoE:** poll cadence aligned with IPBox (~2 s inputs, ~20 s actuators); configurable via `VELDBUS_POLL_INTERVAL_MS` and `VELDBUS_ACTUATOR_POLL_INTERVAL_MS`.
- **IP0300PoE:** off via unified `C<ch>001030`; on via last-level toggle (`T<ch>991000`); level control unchanged (`S<ch><code>1030`).

## [0.1.1-e2] - 2026-07-12

### Added

- Matter bridge MVP: per actief kanaal een Matter-endpoint (licht, ventilator, schakelaar, dimmer); bediening via Apple Home.
- Relay boot-sweep bij opstart (`I<CH>00`) om fysieke aan/uit-stand te seeden vóór de poll-loop.

### Changed

- Ethernet provisioning: DHCP met fallback naar static `10.10.1.1`; operator override via `network.json` op LittleFS.

## [0.1.0-e1] - 2026-07-11

### Added

- Veldbus MVP: UDP/1001 poll-loop, relay/dimmer commando's, `devices.json` op LittleFS.
- Input poll (`I0000`) en decoder voor idle replies en `B-…E` button-events.
