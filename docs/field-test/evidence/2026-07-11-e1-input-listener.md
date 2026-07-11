# E1.5 input listener — VLAN field test (2026-07-11)

Lab test on Waveshare ESP32-S3-ETH on IPBuilding VLAN. Python gateway **off**; embedded hub at `10.10.1.1` polls relay, dimmer, and input.

## Environment

| Item | Value |
|------|-------|
| Board | Waveshare ESP32-S3-ETH |
| Firmware | `0.1.0-e1` + input poll fix + idle-reply decoder fix |
| Config | Production `devices.json` (3 modules; input `10.10.1.50` with `channels: []`) |
| Python gateway | **Off** (embedded hub sole poller) |

## E1.5 — Input listener

| Check | Pass | Notes |
|-------|------|-------|
| Input module in config | ☑ | `10.10.1.50` type=input |
| `TX poll -> 10.10.1.50 payload=I0000` | ☑ | every ~2 s after input poll fix |
| Physical button press logged | ☑ | multiple press/release pairs |
| Button id decode | ☑ | `2f8185190000df` (matches Python test vector) |

## Findings

### Input poll required for slave mode

Without `I0000` poll to `10.10.1.50`, the IP1100 did not send `B-…E` events to the hub (buttons still worked locally via EEPROM/autonomous mode). Polling input registers the hub as centrale → unsolicited button events arrive.

**Product implication:** a hub can control relay/dimmer only by **not** polling input modules — see design spec §4.3 (actuator-only coexistence).

### Poll idle reply wire format (13 bytes)

After each `I0000` poll the module replies:

```
49025205020400000000000045
= I \x02 R 05 02 04 00 00 00 00 00 00 E
```

Initial firmware expected 14 bytes → `undecoded payload` WARN spam. Fixed in `input.cpp` (field-captured vector + unit test).

## Sample serial (excerpt)

```
I udp_bus: TX poll -> 10.10.1.50 (input) payload=I0000
I udp_bus: RX reply <- 10.10.1.50:1001 (13 bytes) data=422d2f8185190000df03010045
I device_registry: REPLY input 10.10.1.50 button 2f8185190000df: press
I udp_bus: RX reply <- 10.10.1.50:1001 (13 bytes) data=422d2f8185190000df03000045
I device_registry: REPLY input 10.10.1.50 button 2f8185190000df: release
```

## Verdict

| Gate | Pass | Notes |
|------|------|-------|
| **E1.5 input listener accepted** | ☑ yes | press + release on physical button |

**Summary:** Embedded hub on `10.10.1.1` receives and decodes unsolicited `B-…E` input events after polling `10.10.1.50`. Confirms input centrale-claim is poll-gated.
