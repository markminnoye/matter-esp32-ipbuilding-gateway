# E1.3 veldbus commands — VLAN field test (2026-07-11)

Lab test on Waveshare ESP32-S3-ETH on IPBuilding VLAN. Production `devices.json` from gateway `192.168.1.3:8080` (v1.2.4).

## Test targets

| Fysiek | Module | Kanaal | Wire |
|--------|--------|--------|------|
| Gang licht (`Inkom`) | `10.10.1.30` | ch 10 | `S1000` / `C1000` |
| Bureau dimmer | `10.10.1.40` | ch 1 | `S1301030` / `S1001030` |

## E1.3 — Command TX/RX

| Check | Pass | Notes |
|-------|------|-------|
| `TX command ->` gang ON `S1000` | ☑ | serial |
| `TX command ->` gang OFF `C1000` | ☑ | serial |
| `TX command ->` bureau 30% `S1301030` | ☑ | serial |
| `TX command ->` bureau OFF `S1001030` | ☑ | serial |
| Gang lamp toggles physically | ☑ | Inkomhal ch10 |
| Bureau dimmer changes physically | ☑ | dim + uit gezien |
| `REPLY relay/dimmer` after command | ☑ | ch10 + ch1 in `devices.json` |

## Config

- `config/devices.json` fetched via [`scripts/fetch_devices_from_gateway.py`](../../../scripts/fetch_devices_from_gateway.py) from `http://192.168.1.3:8080`
- Registry: 3 modules, 28 active channels

## Verdict

| Gate | Pass | Notes |
|------|------|-------|
| **E1.3 VLAN commands accepted** | ☑ yes | gang relay + bureau dimmer |

**Summary:** Embedded hub op `10.10.1.1` stuurt relay `S`/`C` en dimmer level-commando's; fysieke actuatie bevestigd op gang (ch10) en bureau (ch1). Lab hook (`run_e1_command_test`) verwijderd uit `app_main` na acceptatie.
