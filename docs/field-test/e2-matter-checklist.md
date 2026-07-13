# E2 Matter MVP — field test checklist

Manual verification after flashing E2 firmware to Waveshare ESP32-S3-ETH.

Desk-lab partial acceptance is possible without IPBuilding VLAN (commissioning + endpoint list). Full E2 acceptance requires VLAN or in-cabinet deployment and the **Apple Home MVP-gate**: commissioning + on/off + dim.

**Scope-wijziging (2026-07-12):** Home Assistant Matter is **geen** verplichte MVP-gate meer — beslissing A3 herzien, zie [`phases.md`](phases.md) en design spec §14. HA Matter blijft mogelijk als optionele testlaag, maar bepaalt geen acceptatie.

## Faseroadmap — waar staat E voor?

**E = Embedded** — implementatie- en acceptatiefasen voor **deze ESP32-firmware** (niet de Python-gateway).

| Fase | Naam | Doel | Checklist / evidence |
|------|------|------|----------------------|
| **E0** | Project bootstrap | ESP-IDF + esp-matter skeleton, W5500, dual-netif | [`e0-bootstrap-checklist.md`](e0-bootstrap-checklist.md) |
| **E1** | Veldbus MVP | Payloads, UDP poll, commando's, `devices.json` | [`e1-veldbus-checklist.md`](e1-veldbus-checklist.md) |
| **E2** | Matter MVP | esp-matter bridge, state sync, Apple Home + HA | *dit document* |
| **E3** | Hardening | Watchdog, OTA, PoE (post-MVP) | [`phases.md`](phases.md) |
| **E4** | Gen 2 | Generic Switch, runtime discovery, touch UI | [`phases.md`](phases.md) (deferred) |

**Canonieke beschrijving (taken + done-when):** [`phases.md`](phases.md) (samenvatting) · [design spec §11](../superpowers/specs/2026-07-09-embedded-ipbuilding-gateway-design.md#implementatiefasen) (volledig)

**Projectstatus:** [`AGENTS.md`](../../AGENTS.md)

---

## Prerequisites

- E1 accepted (poll + commando's fysiek gevalideerd)
- ESP-IDF v5.5.4; WiFi SSID/password in menuconfig (thuis-LAN — Matter over WiFi)
- `config/devices.json` met productie-kanalen (`active:true` + `semantic_type`)
- iPhone met Apple Home op hetzelfde thuis-LAN
- **VLAN/cabinet:** Ethernet op IPBuilding VLAN; modules op `.30` (relay), `.40` (dimmer)

## Endpoint-mapping (design spec §8.1)

| `devices.json` | Matter device type | Bediening |
|----------------|-------------------|-----------|
| relay `semantic_type: light` | On/Off Light | aan/uit |
| relay `semantic_type: fan` | Fan (FanControl, Off/High) | aan/uit |
| relay `semantic_type: switch` | On/Off Plug-in Unit | aan/uit |
| dimmer kanaal | Dimmable Light | aan/uit + niveau |

Serial toont bij boot de mapping: `Endpoint <id> <- <ip>:<type>:<ch> "<naam>"`.

---

## E2.1 — Bridge: eerste relay endpoint

| Check | Desk-lab | VLAN/cabinet |
|-------|----------|--------------|
| Serial: `Created N veldbus-mapped endpoints` (N > 0) | ☑ | ☑ |
| Serial: endpoint↔kanaal mapping per regel | ☑ | ☑ |
| Commissioning QR/pincode op serial console | ☑ | ☑ |
| Apple Home commissioning slaagt | ☑ | ☑ |
| Apple Home schakelt 1 relay-kanaal fysiek | N/A | ☑ |

## E2.2 — Alle actieve relay + dimmer endpoints

| Check | Desk-lab | VLAN/cabinet |
|-------|----------|--------------|
| Alle `active:true` kanalen zichtbaar in Apple Home | ☑ | ☑ |
| Kanaalnamen uit `devices.json` zichtbaar als accessoirenaam (NodeLabel) | ☑ | ☑ |
| Lichten als lamp, fans als ventilator, switch als stekker | ☑ | ☑ |
| Dimmer toont aan/uit + helderheidsslider | ☑ | ☑ |
| On/off werkt fysiek op meerdere kanalen | N/A | ☑ |
| Dim-niveau werkt fysiek | N/A | ☑ |

## E2.3 — State sync: veldbus → Matter

| Check | Desk-lab | VLAN/cabinet |
|-------|----------|--------------|
| Serial: `Sync veldbus -> Matter: …` na statuswijziging | N/A | ☑ |
| Wandknop-schakeling zichtbaar in Apple Home (na poll/status) | N/A | ☑ |
| Status na herstart hub convergeert na eerste status-reply | N/A | ☑ |
| **Boot sweep:** relay-kanalen tonen juiste aan/uit-stand direct na commissioning (geen fysieke toggle nodig) | ☑ | ☑ |
| Serial: `Boot sweep relay … -> on/off` per actief kanaal, gevolgd door `Boot sweep complete: N/N …` | ☑ | ☑ |
| Dimmer-kanalen tonen "uit" tot eerste fysieke/commando-event (geaccepteerde native-parity limitatie, geen HTTP) | ☑ | ☑ |

**Let op:** de reguliere poll-ack (`P000000000`, `I0154999`) bevat geen kanaalstatus — externe schakelingen verschijnen zodra de module een statusbericht stuurt na een `S`/`C`-commando. De **boot-sweep** (`I<CH>00` → `I000<CH><state>`, vóór de eerste poll) is hiervan losstaand en levert de relay-startstatus al vóór het eerste event; zie [`phases.md`](phases.md#e3--hardening-post-mvp).

## E2.4 — Command path: Matter → veldbus

| Check | Desk-lab | VLAN/cabinet |
|-------|----------|--------------|
| Serial: `Matter -> veldbus: ep=… on_off=…` bij toggle | ☑ (TX zichtbaar) | ☑ |
| Toggle vanuit Apple Home schakelt fysiek | N/A | ☑ |
| Dim vanuit Apple Home zet fysiek niveau | N/A | ☑ |
| Dimmer "aan" na "uit" herstelt het slider-niveau (niet 100%) | N/A | ☑ |
| **Fan:** toggle én speed-slider vanuit Apple Home schakelen relay — bevestig of PercentSetting-writes ook FanMode zetten (aanname fan-server) | N/A | ☑ |
| Geen command-echo-loop (sync-update triggert geen TX) | ☑ | ☑ |

## MVP-gate (beslist A3, herzien 2026-07-12)

| Gate | Criteria | Status |
|------|----------|--------|
| **Apple Home** | Commissioning + on/off + dim | ☑ |
| ~~HA Matter~~ | ~~Commissioning + on/off + dim~~ | **out of scope** (A3 herzien 2026-07-12 — Apple Home is het enige controllerpad voor dit product) |

---

## Build / flash

```bash
cp config/examples/devices.example.json config/devices.json   # of productie-export
idf.py build
idf.py -p /dev/cu.usbmodem* flash monitor
```

Commissioning-code verschijnt op serial bij eerste boot (of na factory reset: `idf.py erase-flash`).

---

## Serial log reference

| Direction | Example | Meaning |
|-----------|---------|---------|
| Boot | `Boot sweep relay 10.10.1.30 ch10 -> on` | UDP status-sweep vóór poll-loop (`I<CH>00`), seedt registry zonder listener-trigger |
| Boot | `Boot sweep complete: 24/24 relay channel(s) seeded` | Sweep-samenvatting, vóór `matter::init()` |
| Boot | `Aggregator endpoint 1 (bridge root)` | Node presenteert zich als Matter-bridge |
| Boot | `Endpoint 2 <- 10.10.1.30:relay:10 "Inkom"` | Veldbus-kanaal → Matter endpoint mapping; naam = NodeLabel |
| Boot | `Created 28 veldbus-mapped endpoints` | Bridge klaar vóór `esp_matter::start` |
| Boot | `State sync active (veldbus -> Matter attributes)` | E2.3 listener geregistreerd |
| Matter → veldbus | `Matter -> veldbus: ep=2 "Inkom" on_off=1` | Controller-commando; direct gevolgd door `TX command ->` |
| Matter → veldbus | `Matter -> veldbus: ep=5 "Bureau" level=127 (50%)` | Dim-commando (Matter 1–254 → veldbus 0–100%) |
| veldbus → Matter | `Sync veldbus -> Matter: 10.10.1.30:relay:10 ep=2 on` | Statuswijziging op de bus → attribute update |
| Commissioning | `Commissioning complete` | Fabric toegevoegd |

**Gedragsnotities:**

- Bridge-topologie: Aggregator-endpoint + per kanaal een Bridged Node met `NodeLabel` (= `name` uit `devices.json`) en `UniqueID` (= entity-id `ip:type:ch`). Controllers tonen elk kanaal als los accessoire met die naam.
- Endpoint-id's worden in `devices.json`-volgorde toegewezen — volgorde bij voorkeur niet wijzigen na commissioning; de `UniqueID` helpt controllers de identiteit te behouden, maar niet elke controller gebruikt die.
- `room` uit `devices.json` wordt **niet** via Matter geëxporteerd: Matter kent geen standaard room-attribuut dat Apple Home of HA overneemt — kamers wijs je toe in de controller zelf.
- Dimmer aan/uit (OnOff-toggle) stuurt native `T<ch>991000` (input-module dialect, `2026-05-17_dimmer_I0154xxx_full_decode.md`): de module flipt off↔laatst-niveau-geheugen — zelfde semantiek als de fysieke knop; overleeft hub-reboots. Expliciet niveau (slider) = `S<ch><code>1030`. `CurrentLevel` blijft bij "uit" op het laatste niveau staan (Matter-semantiek) en wordt na de toggle-statusreply gesynct met wat de module echt herstelde.
- Fan-endpoints gebruiken FanControl met sequence Off/High; Apple toont een ventilator-tegel. FanMode én PercentSetting worden beide afgehandeld (defensief — relay-commando's zijn absoluut, dubbel commando is onschadelijk); veldtest moet bevestigen wat Apple daadwerkelijk schrijft.
- De echo-guard (`s_sync_in_progress`) voorkomt dat een veldbus→Matter sync opnieuw een veldbus-commando triggert.
- **Relay boot-status:** `udp_bus_manager::start()` doet vóór de poll/rx-tasks een UDP-only sweep (`I<CH>00` → `I000<CH><state>`, native IPBox-gedrag) over alle actieve relay-kanalen en seedt `DeviceRegistry` zonder listeners te triggeren; `create_endpoints()` leest die seed voor de initiële `on_off`-waarde. **Dimmer heeft geen analoog** — dat blijft bewust "uit" tot het eerste event (geen HTTP-workaround).

---

## Verdict

| Gate | Criteria | Status |
|------|----------|--------|
| **Desk-lab E2 code** | Endpoints aangemaakt, commissioning start, geen crash | ☑ |
| **Apple Home MVP-gate** | Commissioning + on/off + dim fysiek | ☑ |
| ~~HA Matter MVP-gate~~ | ~~Commissioning + on/off + dim fysiek~~ | **out of scope** (2026-07-12) |
| **E2.3 state sync** | Externe schakeling zichtbaar in controller | ☑ |
| **Full E2** | Alle gates ☑ + evidence-document | ☑ — [`2026-07-12-e2-matter-mvp.md`](evidence/2026-07-12-e2-matter-mvp.md) |

**E2 gesloten (2026-07-12).**
