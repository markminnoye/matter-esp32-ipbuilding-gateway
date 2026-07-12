# Embedded firmware phases (E0–E4)

**E = Embedded** — implementatie- en acceptatiefasen voor de ESP32-firmware in deze repo (niet de Python-gateway).

Volledige taken en “done when”-criteria staan in het [design spec §11](../superpowers/specs/2026-07-09-embedded-ipbuilding-gateway-design.md#implementatiefasen).

| Fase | Naam | Doel | Veldtest |
|------|------|------|----------|
| **E0** | Project bootstrap | ESP-IDF + esp-matter skeleton, W5500, dual-netif | [e0-bootstrap-checklist.md](e0-bootstrap-checklist.md) — **closed** |
| **E1** | Veldbus MVP | Payloads, UDP poll, commando's, `devices.json` | [e1-veldbus-checklist.md](e1-veldbus-checklist.md) |
| **E2** | Matter MVP | esp-matter bridge, state sync, Apple Home + HA | [e2-matter-checklist.md](e2-matter-checklist.md) |
| **E3** | Hardening | Watchdog, OTA, PoE (post-MVP) | design spec §11 |
| **E4** | Gen 2 | Generic Switch, discovery, touch UI | design spec §11 (deferred) |

## E0 — Project bootstrap

| # | Taak | Done when |
|---|------|-----------|
| E0.1 | Skeleton-firmware in deze repo | Skeleton buildt in CI |
| E0.2 | ESP-IDF + esp-matter hello op ESP32-S3-ETH | Serial log + chip-id |
| E0.3 | W5500 link-up op IPBuilding VLAN | LINK LED + ping vanaf LAN |
| E0.4 | Dual-netif: eth `10.10.1.1`, WiFi STA thuis-LAN | Beide interfaces up |

## E1 — Veldbus MVP

| # | Taak | Done when |
|---|------|-----------|
| E1.1 | Port `payloads/relay` + `payloads/dimmer` | Unit tests + loopback |
| E1.2 | UDP poll naar `10.10.1.30` / `.40` | PCAP of mirror bevestigt `P0000` |
| E1.3 | Relay `S`/`C` + dimmer level command | Fysiek licht schakelt |
| E1.4 | `devices.json` laden van LittleFS | Alleen `active:true` kanalen in registry |
| E1.5 | Input listener `B-…E` (log only) | Serial log bij druk knop — ☑ [`2026-07-11-e1-input-listener.md`](evidence/2026-07-11-e1-input-listener.md) |

## E2 — Matter MVP

| # | Taak | Done when |
|---|------|-----------|
| E2.1 | esp-matter bridge: 1 relay endpoint | Apple Home schakelt 1 kanaal |
| E2.2 | Alle actieve relay + dimmer endpoints | Commissioning + on/off + dim |
| E2.3 | State sync: veldbus → Matter attribute | Status match na poll |
| E2.4 | Command path: Matter → veldbus | Toggle vanuit Apple Home |

**MVP-gate:** commissioning + bediening in **Apple Home** én **Home Assistant Matter**.

## E3 — Hardening (post-MVP)

- **Ethernet provisioning:** factory default **DHCP → fallback static `10.10.1.1`**, `network.json`, web UI — [#3](https://github.com/markminnoye/matter-esp32-ipbuilding-gateway/issues/3)
- **Matter factory reset** via Identify-knop — [#2](https://github.com/markminnoye/matter-esp32-ipbuilding-gateway/issues/2)
- Bind-recovery voor `10.10.1.1`
- Watchdog + crash logging
- OTA (esp-matter OTA)
- PoE-module test
- **Initiële status na boot (geparkeerd uit E2-veldtest 2026-07-12):** na een herstart
  toont Matter alle kanalen als "uit" tot een kanaal fysiek of via commando wijzigt.
  Oorzaak is een protocol-beperking (RE 2026-06-02): de poll-reply (`P000000000`,
  `I0154999`) bevat géén kanaalstatus — modules melden hun stand alleen na een
  `S`/`C`-commando of bij een statuswijziging, en een "geef alle standen"-query is
  niet ge-RE'd. Status convergeert per kanaal bij het eerste event; workaround na
  herstart is elk kanaal één keer schakelen. **Oplossing bekend:** de Python-gateway
  hydrateert sinds [`b592105f`](https://github.com/markminnoye/IPBuilding-Gateway/commit/b592105f)
  (2026-06-18) de registry bij opstart via HTTP `GET /api.html?method=statuses`
  op elke relay/dimmer-module. **Actie E3:** zelfde boot-hydration porten naar de
  embedded firmware (esp_http_client naar `.30`/`.40`, seed zonder listeners te
  triggeren).

## E4 — Gen 2 (deferred)

Generic Switch (IP1100), runtime ARP discovery, EEPROM-sync, touch UI, cover entities.

---

**Projectstatus:** [`AGENTS.md`](../../AGENTS.md)  
**Design spec (volledig):** [`2026-07-09-embedded-ipbuilding-gateway-design.md`](../superpowers/specs/2026-07-09-embedded-ipbuilding-gateway-design.md)
