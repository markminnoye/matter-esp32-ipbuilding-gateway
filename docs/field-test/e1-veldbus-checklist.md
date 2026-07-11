# E1 Veldbus MVP — field test checklist

Desk-lab partial acceptance is possible without IPBuilding VLAN. Full E1 acceptance requires VLAN or in-cabinet deployment.

## E1.1 — Payload port

| Check | Desk-lab | VLAN/cabinet |
|-------|----------|--------------|
| Relay encode/decode matches Python vectors | ☑ Unity tests (`test/payload`) | — |
| Dimmer encode/decode matches Python vectors | ☑ Unity tests | — |
| Input `B-…E` decode matches Python vectors | ☑ Unity tests | — |

## E1.2 — UDP poll

| Check | Desk-lab | VLAN/cabinet |
|-------|----------|--------------|
| Serial: `UdpBusManager started on 10.10.1.1:1001` | ☐ | ☐ |
| Serial: `Poll 10.10.1.30 (relay)` every ~2 s | ☐ | ☐ |
| PCAP/mirror confirms `P0000` on wire | N/A | ☐ |

## E1.3 — Relay/dimmer commands

| Check | Desk-lab | VLAN/cabinet |
|-------|----------|--------------|
| Command encode paths compile and log | ☐ | ☐ |
| Physical relay toggles from hub command | N/A | ☐ |
| Dimmer level changes physically | N/A | ☐ |

## E1.4 — devices.json + registry

| Check | Desk-lab | VLAN/cabinet |
|-------|----------|--------------|
| LittleFS mount or embedded fallback | ☐ | ☐ |
| Serial: `Loaded N modules (M active with channels)` | ☐ | ☐ |
| Inactive channels excluded from poll list | ☐ | ☐ |

## E1.5 — Input listener

| Check | Desk-lab | VLAN/cabinet |
|-------|----------|--------------|
| Input decode unit tests pass | ☐ | ☐ |
| Serial log on physical button press | N/A | ☐ |

## Build / flash

```bash
cp config/devices.example.json config/devices.json   # first time
idf.py build
idf.py -p /dev/cu.usbmodem* flash monitor
```

Payload unit tests (separate test app):

```bash
cd test/payload
idf.py build
```

## Verdict

- **Desk-lab E1 code accepted:** E1.1 + E1.4 + poll loop logs (no module replies OK)
- **Full E1 accepted:** above + PCAP + physical actuation + input button log
