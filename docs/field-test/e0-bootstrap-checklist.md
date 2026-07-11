# E0 bootstrap — field test checklist

Manual verification after flashing E0 firmware to Waveshare ESP32-S3-ETH.

## Prerequisites

- ESP-IDF v5.5.4 installed; WiFi SSID/password set in menuconfig
- Board connected via USB-C; Ethernet cable to IPBuilding VLAN (UniFi port)
- Serial monitor at 115200 baud

## E0.1 — Skeleton build

| Check | Pass |
|-------|------|
| `idf.py build` succeeds locally | ☐ |
| GitHub Actions CI build green | ☐ |

## E0.2 — Matter hello (serial)

| Check | Pass |
|-------|------|
| Serial shows chip info + free heap | ☐ |
| Log: `Matter stack started` (no reboot loop) | ☐ |
| Log: `Placeholder On/Off Light endpoint_id=…` | ☐ |

Full Apple Home commissioning is **E2** — E0 only validates stack startup.

## E0.3 — W5500 Ethernet

| Check | Pass |
|-------|------|
| LINK LED on RJ45 port | ☐ |
| Serial: `Ethernet link up` | ☐ |
| Serial: `ETH: ip=10.10.1.1` | ☐ |
| `ping 10.10.1.1` from host on IPBuilding VLAN | ☐ |

## E0.4 — Dual-netif WiFi

| Check | Pass |
|-------|------|
| Serial: `WiFi connected, RSSI=… dBm` | ☐ |
| Serial: `WiFi: ip=192.168.x.x` (DHCP) | ☐ |
| Both ETH and WiFi IPs visible in log | ☐ |

**H1 (open):** note RSSI in metal fuse cabinet — compare with desk test.

## Evidence

Store results in a dated markdown file when running on real hardware, e.g. `docs/field-test/evidence/YYYY-MM-DD-e0-bootstrap.md`.

Shared veldbus capture workflows: [IPBuilding-Gateway workflows](https://github.com/markminnoye/IPBuilding-Gateway/tree/main/resources_and_docs/workflows).
