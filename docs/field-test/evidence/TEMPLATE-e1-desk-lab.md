# E1 veldbus MVP — field test evidence template

Copy this file to `docs/field-test/evidence/YYYY-MM-DD-e1-<context>.md` (e.g. `2026-07-11-e1-desk-lab.md`, `2026-08-01-e1-vlan-commands.md`).

**Checklist:** [`../e1-veldbus-checklist.md`](../e1-veldbus-checklist.md)  
**Faseroadmap (E0–E4):** [`../phases.md`](../phases.md) · [design spec §11](../../superpowers/specs/2026-07-09-embedded-ipbuilding-gateway-design.md#implementatiefasen)

---

## Environment

| Item | Value |
|------|-------|
| Date | YYYY-MM-DD |
| Board | Waveshare ESP32-S3-ETH (ESP32-S3R8 + W5500) |
| ESP-IDF | v5.5.4 |
| Git branch / commit | |
| Firmware version | `0.1.0-e1` |
| Serial port | `/dev/cu.usbmodem…` |
| WiFi | SSID …, IP `192.168.x.x`, RSSI … dBm |
| Ethernet | static `10.10.1.1`; VLAN yes/no |
| **Lab constraint** | e.g. desk without IPBuilding VLAN, or full VLAN |

---

## E1.1 — Payload port

| Check | Pass | Notes |
|-------|------|-------|
| Main firmware `idf.py build` green | ☐ | |
| `test/payload` Unity build (+ run if flashed) | ☐ | relay, dimmer, input, poll-echo, idle |
| Relay poll echo `P000000000` test | ☐ | |

---

## E1.4 — Config + registry

| Check | Pass | Notes |
|-------|------|-------|
| Config loaded | ☐ | `Parsed N modules from config` |
| Registry active channels | ☐ | `Loaded N modules (M active with channels)` |
| Inactive channels excluded from poll | ☐ | |

---

## E1.2 — Poll loop

| Check | Pass | Notes |
|-------|------|-------|
| `UdpBusManager started on 10.10.1.1:1001` | ☐ | |
| Poll logs every ~2 s | ☐ | relay `.30` + dimmer `.40` |
| Boot `sendto errno=118` absent (eth link wait) | ☐ | |
| Module replies (VLAN only) | ☐ | see VLAN section |

### VLAN poll (skip if desk-only)

| Check | Pass | Notes |
|-------|------|-------|
| Relay poll reply `P000000000` | ☐ | poll echo — not channel state |
| Dimmer poll reply `I0154999` | ☐ | idle heartbeat — not level |
| No misleading `undecoded` warnings | ☐ | |
| PCAP/mirror `P0000` on wire | ☐ | |

---

## E1.3 — Relay/dimmer commands (VLAN/cabinet)

| Check | Pass | Notes |
|-------|------|-------|
| Relay toggles physically from hub command | ☐ | |
| Dimmer level changes physically | ☐ | |
| Serial `REPLY relay … state=…` after command | ☐ | |
| Serial `REPLY dimmer … level=…` after dim | ☐ | |

---

## E1.5 — Input listener (VLAN/cabinet)

| Check | Pass | Notes |
|-------|------|-------|
| Serial log on button press (`B-…E`) | ☐ | log-only in Gen 1 |

---

## E0 carry-over (still valid)

| Check | Pass | Notes |
|-------|------|-------|
| Dual-netif ETH + WiFi | ☐ | |
| WiFi RSSI (H1 cabinet note) | ☐ | |
| Matter placeholder + commissioning window | ☐ | full commissioning = **E2** |
| Stability (>5 min, no reboot loop) | ☐ | heap … |

---

## Sample serial (paste excerpt)

```
(paste relevant boot + poll + command lines)
```

---

## Verdict

| Gate | Pass | Notes |
|------|------|-------|
| **Desk-lab E1 code accepted** | ☐ yes / ☐ no | E1.1 + E1.4 + poll loop |
| **VLAN poll TX/RX accepted** | ☐ yes / ☐ no / N/A | |
| **Full E1 accepted** | ☐ yes / ☐ no | + PCAP + E1.3 + E1.5 |

**Summary:** _(one paragraph: what was tested, what remains)_
