# E1 Veldbus MVP — field test checklist

Manual verification after flashing E1 firmware to Waveshare ESP32-S3-ETH.

Desk-lab partial acceptance is possible without IPBuilding VLAN. Full E1 acceptance requires VLAN or in-cabinet deployment.

## Faseroadmap — waar staat E voor?

**E = Embedded** — implementatie- en acceptatiefasen voor **deze ESP32-firmware** (niet de Python-gateway).

| Fase | Naam | Doel | Checklist / evidence |
|------|------|------|----------------------|
| **E0** | Project bootstrap | ESP-IDF + esp-matter skeleton, W5500, dual-netif | [`e0-bootstrap-checklist.md`](e0-bootstrap-checklist.md) |
| **E1** | Veldbus MVP | Payloads, UDP poll, commando's, `devices.json` | *dit document* |
| **E2** | Matter MVP | esp-matter bridge, state sync, Apple Home + HA | *(nog te maken)* |
| **E3** | Hardening | Watchdog, OTA, PoE (post-MVP) | [`phases.md`](phases.md) |
| **E4** | Gen 2 | Generic Switch, runtime discovery, touch UI | [`phases.md`](phases.md) (deferred) |

**Canonieke beschrijving (taken + done-when):** [`phases.md`](phases.md) (samenvatting) · [design spec §11](../superpowers/specs/2026-07-09-embedded-ipbuilding-gateway-design.md#implementatiefasen) (volledig)

**Projectstatus:** [`AGENTS.md`](../../AGENTS.md) — korte samenvatting per fase.

**Evidence invullen:** kopieer [`evidence/TEMPLATE-e1-desk-lab.md`](evidence/TEMPLATE-e1-desk-lab.md) naar `evidence/YYYY-MM-DD-e1-*.md`.

---

## Prerequisites

- E0 accepted (dual-netif, Matter stack starts)
- ESP-IDF v5.5.4; WiFi SSID/password in menuconfig
- `config/devices.json` present (copy from `config/devices.example.json` on first build)
- Serial monitor at 115200 baud
- **Desk-lab:** Ethernet link-up optional; no module replies expected
- **VLAN/cabinet:** Ethernet on IPBuilding VLAN (`10.10.1.0/24`); modules at `.30` (relay), `.40` (dimmer)

---

## E1.1 — Payload port

| Check | Desk-lab | VLAN/cabinet |
|-------|----------|--------------|
| Relay encode/decode matches Python vectors | ☑ Unity tests (`test/payload`) | — |
| Dimmer encode/decode matches Python vectors | ☑ Unity tests | — |
| Input `B-…E` decode matches Python vectors | ☑ Unity tests | — |
| Relay poll echo `P000000000` recognized (not undecoded) | ☑ Unity test | ☑ serial (no WARN) |
| Dimmer idle `I0154999` recognized (not undecoded) | ☑ Unity test | ☑ serial (no WARN) |

## E1.2 — UDP poll

| Check | Desk-lab | VLAN/cabinet |
|-------|----------|--------------|
| Serial: `UdpBusManager started on 10.10.1.1:1001` | ☑ | ☑ [`2026-07-11-e1-desk-lab.md`](evidence/2026-07-11-e1-desk-lab.md) |
| Serial: poll every ~2 s (relay `.30` + dimmer `.40`) | ☑ | ☑ |
| Serial: `TX poll ->` / `RX reply <-` | N/A (no replies) | ☑ poll-ack + idle heartbeat |
| No boot `sendto errno=118` after eth link wait | ☐ retest | ☐ retest |
| PCAP/mirror confirms `P0000` on wire | N/A | ☐ |

## E1.3 — Relay/dimmer commands

| Check | Desk-lab | VLAN/cabinet |
|-------|----------|--------------|
| Command encode paths compile and log | ☐ | ☐ |
| Physical relay toggles from hub command | N/A | ☐ |
| Dimmer level changes physically | N/A | ☐ |
| Serial: `REPLY relay … state=on/off` after command | N/A | ☐ |
| Serial: `REPLY dimmer … level=N` after dim command | N/A | ☐ |

## E1.4 — devices.json + registry

| Check | Desk-lab | VLAN/cabinet |
|-------|----------|--------------|
| LittleFS mount or embedded fallback | ☑ | ☑ 2 modules parsed |
| Serial: `Loaded N modules (M active with channels)` | ☑ | ☑ 2 active |
| Inactive channels excluded from poll list | ☐ | ☐ |

## E1.5 — Input listener

| Check | Desk-lab | VLAN/cabinet |
|-------|----------|--------------|
| Input decode unit tests pass | ☑ (see E1.1) | — |
| Serial log on physical button press (`B-…E`) | N/A | ☐ |

---

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

---

## Serial log reference (VLAN)

Poll replies are **not** channel state — they confirm the module is alive (matches Python `module_status.py`).

| Direction | Example | Meaning |
|-----------|---------|---------|
| TX poll | `TX poll -> 10.10.1.30 payload=P0000` | Hub poll to relay module |
| RX relay | `RX reply <- 10.10.1.30 … data=P000000000` | Poll echo (`P` + 9 digits) — no on/off state |
| TX poll | `TX poll -> 10.10.1.40 payload=I9900` | Hub poll to dimmer module |
| RX dimmer | `RX reply <- 10.10.1.40 … data=I0154999` | Idle heartbeat — no level |
| State (E1.3) | `REPLY relay … ch=1 state=on` | After `S`/`C` command or unsolicited status |
| State (E1.3) | `REPLY dimmer … ch=1 level=30` | After dim command or level reply (`I0154030`) |

At default log level (`INFO`), poll-ack and idle heartbeat are logged at `DEBUG` only — no `undecoded` warnings for these.

Boot order: Ethernet **link up** must occur before veldbus starts sending (avoids early `sendto errno=118`).

To see poll-ack lines:

```bash
idf.py monitor --print-filter="device_registry:D"
```

---

## Verdict

| Gate | Criteria | Status |
|------|----------|--------|
| **Desk-lab E1 code** | E1.1 + E1.4 + poll loop logs (no module replies OK) | ☑ [`2026-07-11-e1-desk-lab.md`](evidence/2026-07-11-e1-desk-lab.md) |
| **VLAN poll TX/RX** | E1.2 poll-ack + idle on wire | ☑ same evidence |
| **Full E1** | above + PCAP + E1.3 physical actuation + E1.5 button log | ☐ blocked on E1.3 / E1.5 |
