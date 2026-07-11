# E0 bootstrap — evidence (2026-07-11)

Lab test on Waveshare ESP32-S3-ETH at desk. Firmware branch `development`, commit `fae0590` (+ local rebuild with WiFi via menuconfig).

## Environment

| Item | Value |
|------|-------|
| Board | Waveshare ESP32-S3-ETH (ESP32-S3R8 + W5500) |
| ESP-IDF | v5.5.4 (EIM) |
| Serial port | `/dev/cu.usbmodem1501` |
| WiFi | Home LAN (2.4 GHz), credentials via menuconfig |
| Ethernet | Link-up on desk; static `10.10.1.1` configured in firmware |
| **Lab constraint** | No IPBuilding VLAN (`10.10.1.0/24`) on desk — veldbus ping test deferred |

## E0.1 — Skeleton build

| Check | Pass |
|-------|------|
| `idf.py build` succeeds locally | ☑ |
| GitHub Actions CI build green | ☐ (not pushed yet) |

## E0.2 — Matter hello (serial)

| Check | Pass |
|-------|------|
| Serial shows chip info + free heap | ☑ |
| Log: `Matter stack started` (no reboot loop) | ☑ |
| Log: `Placeholder On/Off Light endpoint_id=1` | ☑ |
| Log: `Commissioning window opened` | ☑ |

## E0.3 — W5500 Ethernet

| Check | Pass | Notes |
|-------|------|-------|
| LINK LED on RJ45 port | ☑ | |
| Serial: `Ethernet link up` | ☑ | |
| Serial: `ETH: ip=10.10.1.1` | ☑ | Static config as designed |
| `ping 10.10.1.1` from IPBuilding VLAN | ☐ | **Deferred** — desk has home LAN only |

## E0.4 — Dual-netif WiFi

| Check | Pass | Notes |
|-------|------|-------|
| Serial: `WiFi connected, RSSI=… dBm` | ☑ | RSSI **-64 dBm** (desk) |
| Serial: `WiFi: ip=192.168.x.x` (DHCP) | ☑ | `192.168.1.46` |
| Both ETH and WiFi IPs visible in log | ☑ | |

## H1 (open)

Desk RSSI: **-64 dBm**. Cabinet test still pending.

## Interim lab mode (2026-07-11)

Until IPBuilding VLAN is available at the test bench:

- **Continue development on home LAN** (WiFi Matter path, serial debug).
- Ethernet remains configured as field-bus hub IP (`10.10.1.1`); no veldbus modules on desk network.
- **E1 veldbus tests** require IPBuilding VLAN or in-cabinet deployment — not blocking E1 code work (payload port, UDP stack, LittleFS).

## Verdict

**E0 accepted** for desk lab: dual-netif up, Matter stack stable, commissioning window open. VLAN ping and cabinet RSSI remain follow-ups before production install.
