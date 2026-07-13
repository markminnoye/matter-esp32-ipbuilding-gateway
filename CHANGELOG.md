# Changelog

All notable changes to the embedded IPBuilding Gateway firmware are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

Firmware version is `PROJECT_VER` in the root `CMakeLists.txt` (Matter software version).

## [Unreleased]

### Changed

- **Veldbus-poll cadans gelijk aan IPBox:** input-modules elke ~2 s (`I0000`); relay en dimmer elke ~20 s (`P0000` / `I9900`). Twee menuconfig-opties: `VELDBUS_POLL_INTERVAL_MS` (input, default 2000) en `VELDBUS_ACTUATOR_POLL_INTERVAL_MS` (relay/dimmer, default 20000). Zelfde gedrag als de Python gateway (`actuator_poll_interval`).

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
