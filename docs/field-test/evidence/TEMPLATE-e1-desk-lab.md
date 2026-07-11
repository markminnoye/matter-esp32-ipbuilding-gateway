# E1 veldbus MVP — desk-lab evidence template

Copy to `docs/field-test/evidence/YYYY-MM-DD-e1-desk-lab.md` when validating.

## Environment

| Item | Value |
|------|-------|
| Board | Waveshare ESP32-S3-ETH |
| Firmware version | `0.1.0-e1` |
| Lab constraint | No IPBuilding VLAN — hardware tests deferred |

## E1.1 Payload tests

| Check | Pass | Notes |
|-------|------|-------|
| `test/payload` Unity build green | ☐ | |

## E1.4 Config + registry

| Check | Pass | Notes |
|-------|------|-------|
| Config loaded (LittleFS or fallback) | ☐ | |
| Active channel count matches expectation | ☐ | |

## E1.2 Poll loop (desk)

| Check | Pass | Notes |
|-------|------|-------|
| `UdpBusManager started on 10.10.1.1:1001` | ☐ | |
| Poll logs every ~2 s | ☐ | No replies expected without VLAN |

## Deferred until VLAN/cabinet

- PCAP `P0000`
- Physical relay/dimmer actuation
- Input button serial log

## Verdict

**Desk-lab E1 code accepted:** ☐ yes / ☐ no
