# Embedded IPBuilding Gateway — agent brief

**Doel:** standalone **ESP32-S3 firmware** die de veldbus-hub-rol vervult (UDP/1001 naar relay/dimmer/input modules) en **Matter** als northbound exporteert — zonder Python-gateway op het board (Gen 1).

**Niet het doel:** WebSocket `/ws`, REST `/api/v1/`, IPBox REST-shim, companion-integratie, of IPBox-pariteit (sferen, knop→relay mapping). Logica hoort in **Home Assistant** of **Apple Home**.

**Productprincipe:** dunne veldbus-hub (pollen, commando's, `B-…E` doorgeven) + Matter-endpoints. Geen tweede projectdatabase in firmware.

**Architectuur (goedgekeurd):** [`docs/superpowers/specs/2026-07-09-embedded-ipbuilding-gateway-design.md`](docs/superpowers/specs/2026-07-09-embedded-ipbuilding-gateway-design.md) — Deployment C / Fase 12; dual-netif (Ethernet veldbus + WiFi Matter); Gen 1 relay + dimmer endpoints.

**Protocol-spec (port-bron):** [`IPBuilding-Gateway/gateway/payloads/`](https://github.com/markminnoye/IPBuilding-Gateway/tree/main/gateway/payloads) — handmatige sync, geen submodule (beslissing A4).

**Diepte-facts (Python-repo):** [`IPBUILDING_KNOWLEDGE.md`](https://github.com/markminnoye/IPBuilding-Gateway/blob/main/resources_and_docs/IPBUILDING_KNOWLEDGE.md) — niet herschrijven in de chat; verwijs naar sectie.

**Token-/contextbeleid:** [`docs/context-policy.md`](docs/context-policy.md).

---

## Status

**E0 bootstrap — closed (2026-07-11):**
- Firmware skeleton: ESP-IDF + managed esp-matter component
- W5500 board layer + dual-netif network manager (ETH `10.10.1.1` + WiFi STA)
- Matter placeholder On/Off Light endpoint (E2 replaces with veldbus-mapped endpoints)
- CI: GitHub Actions `idf.py build` for esp32s3
- **Veldtest evidence:** [`docs/field-test/evidence/2026-07-11-e0-bootstrap.md`](docs/field-test/evidence/2026-07-11-e0-bootstrap.md)

**E1 veldbus MVP — E1.5 input accepted (2026-07-11); full E1 open on PCAP (E1.2):**
- Payload port, ConfigStore, DeviceRegistry, UdpBusManager + command TX API (`send_relay_command`, `send_dimmer_command`)
- Poll-ack decoder fix: `P000000000` / `I0154999` no longer logged as undecoded
- Input poll fix: IP1100 included in poll loop; idle reply 13-byte decoder
- Production `devices.json` from Python gateway `192.168.1.3:8080` (28 channels)
- **Veldtest evidence:** desk/poll [`2026-07-11-e1-desk-lab.md`](docs/field-test/evidence/2026-07-11-e1-desk-lab.md) · commands [`2026-07-11-e1-vlan-commands.md`](docs/field-test/evidence/2026-07-11-e1-vlan-commands.md) · input [`2026-07-11-e1-input-listener.md`](docs/field-test/evidence/2026-07-11-e1-input-listener.md)
- **Nog open:** PCAP (E1.2)
- **RE bevinding:** input centrale-claim is poll-gated → actuator-only hub mogelijk (design spec §4.3); feature [IPBuilding-Gateway #16](https://github.com/markminnoye/IPBuilding-Gateway/issues/16) — **Python gateway eerst**, embedded later

**E2 Matter MVP — code implemented (2026-07-11); veldtest open:**
- MatterBridge: per actief kanaal een endpoint (light→On/Off Light, fan→Fan Off/High, switch→Plug-in Unit, dimmer→Dimmable Light); placeholder-endpoint verwijderd
- Command path (E2.4): attribute PRE_UPDATE → `send_relay_command`/`send_dimmer_command`/`send_dimmer_off`; echo-guard tegen sync-loops
- State sync (E2.3): DeviceRegistry state-change listeners → `attribute::update` onder chip-stack lock
- **Veldtest:** [`docs/field-test/e2-matter-checklist.md`](docs/field-test/e2-matter-checklist.md) — MVP-gates Apple Home + HA Matter nog open

**Volgende stap:** E2-veldtest (commissioning + bediening in Apple Home én HA Matter)

**Open beslissingen:**
- **H1:** WiFi-bereik in metalen zekeringskast (desk RSSI -64 dBm; cabinet test open)
- **H2:** Productie-hardware: DIN-rail board vs gemonteerd dev-board (na E2-acceptatie)

**Lab-hardware (bevestigd):** Waveshare **ESP32-S3-ETH** (ESP32-S3R8 + W5500).

**Canonieke RE-status (Python-repo):** [`RE_STATE.md`](https://github.com/markminnoye/IPBuilding-Gateway/blob/main/resources_and_docs/RE_STATE.md) — veldbus wire afgerond 2026-05-22.

---

## Scope (Gen 1 MVP)

| In scope | Buiten scope (Gen 1) |
|----------|----------------------|
| Hub op `10.10.1.1`, UDP/1001 | WebSocket/REST op chip |
| Port `gateway/payloads/` (relay, dimmer) | Runtime ARP auto-discovery |
| `devices.json` op flash (LittleFS) | Cover/screen entities |
| Matter: On/Off Light, Fan, Plug-in Unit, Dimmable Light | Matter Generic Switch (IP1100) → Gen 2 |
| Dual-netif: eth veldbus + WiFi Matter | EEPROM-sync / `saveAutonomy` |
| Input listener `B-…E` (log only) | Touch UI |

**MVP-gates (E2):** commissioning + bediening in **Apple Home** én **Home Assistant Matter** (beslissing A3).

---

## Volgende focus (implementatie)

1. **E0 — Project bootstrap** — ESP-IDF + esp-matter skeleton, W5500 link-up, dual-netif
2. **E1 — Veldbus MVP** — port payloads, poll, relay/dimmer commando's, config laden
3. **E2 — Matter MVP** — esp-matter bridge, state sync veldbus ↔ Matter
4. **E3 — Hardening** — watchdog, OTA, PoE-test (post-MVP)

Zie [implementatiefasen (E0–E4)](docs/field-test/phases.md) — samenvatting; detail in [design spec §11](docs/superpowers/specs/2026-07-09-embedded-ipbuilding-gateway-design.md#implementatiefasen).

---

## Netwerk (referentie)

| Rol | IP | Poort / protocol |
|-----|-----|------------------|
| Embedded hub (eth) | `10.10.1.1` | UDP/1001 |
| Relays | `10.10.1.30` | 1001 |
| Dimmers | `10.10.1.40` | 1001 |
| Inputs | `10.10.1.50` | 1001 |
| Matter (WiFi) | thuis-LAN `192.168.x.x` | Matter over WiFi |

**Parallel spoor A:** Python-gateway op HA-installatie (inter-VLAN routing) — geen IP-conflict met embedded hub op `10.10.1.1`.

**Parallel spoor B:** Pi 3B standalone Python — veldbus-regressie ([hardware-eval](https://github.com/markminnoye/IPBuilding-Gateway/blob/main/resources_and_docs/reference/2026-06-14-deployment-hardware-evaluation.md)).

---

## Gerelateerde repos

| Repo | Rol |
|------|-----|
| [`IPBuilding-Gateway`](https://github.com/markminnoye/IPBuilding-Gateway) | Python gateway + protocol RE + `gateway/payloads/` |
| [`ha-ipbuilding-gateway`](https://github.com/markminnoye/ha-ipbuilding-gateway) | HA companion (Spoor A/B — niet primair voor embedded Gen 1) |
| **`matter-esp32-ipbuilding-gateway`** (deze repo) | C++ ESP-IDF firmware: veldbus + Matter |

---

## Skills

| Skill | Trigger |
|-------|---------|
| **protocol-reverse-engineering** | PCAP, UDP correlatie, payload drift Python ↔ C++ |
| **binary-analysis-patterns** | Payload-structuur, parsers |
| **network-engineer** | VLAN, dual-netif, UniFi |
| **home-assistant-best-practices** | HA Matter commissioning/test |

---

## Doc-index (tier — `docs/context-policy.md`)

- **T0:** `AGENTS.md`, `docs/context-policy.md`
- **T1:** [embedded design spec](docs/superpowers/specs/2026-07-09-embedded-ipbuilding-gateway-design.md)
- **T2:** [`docs/field-test/e0-bootstrap-checklist.md`](docs/field-test/e0-bootstrap-checklist.md), Python-repo RE docs (links)
- **T3:** PCAPs, volledige `devices.json` productie-export

**Code (doelstructuur):** `main/` (app, board, config, registry, veldbus, matter) — zie design spec §10.

---

## Einddoel

**Embedded IPBuilding Gateway:** ESP32-S3 firmware als standalone Matter-bridge in de zekeringskast — veldbus-hub + Matter northbound. Scenes/logica in HA/Apple Home, niet in firmware.
