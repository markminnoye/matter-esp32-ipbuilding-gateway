# E2 Matter MVP — field test checklist

Manual verification after flashing E2 firmware to Waveshare ESP32-S3-ETH.

Desk-lab partial acceptance is possible without IPBuilding VLAN (commissioning + endpoint list). Full E2 acceptance requires VLAN or in-cabinet deployment **and both MVP-gates**: commissioning + bediening in **Apple Home** én **Home Assistant Matter** (design spec beslissing A3).

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
- Home Assistant met Matter-integratie (test-controller)
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
| Serial: `Created N veldbus-mapped endpoints` (N > 0) | ☐ | ☐ |
| Serial: endpoint↔kanaal mapping per regel | ☐ | ☐ |
| Commissioning QR/pincode op serial console | ☐ | ☐ |
| Apple Home commissioning slaagt | ☐ | ☐ |
| Apple Home schakelt 1 relay-kanaal fysiek | N/A | ☐ |

## E2.2 — Alle actieve relay + dimmer endpoints

| Check | Desk-lab | VLAN/cabinet |
|-------|----------|--------------|
| Alle `active:true` kanalen zichtbaar in Apple Home | ☐ | ☐ |
| Kanaalnamen uit `devices.json` zichtbaar als accessoirenaam (NodeLabel) | ☐ | ☐ |
| Lichten als lamp, fans als ventilator, switch als stekker | ☐ | ☐ |
| Dimmer toont aan/uit + helderheidsslider | ☐ | ☐ |
| On/off werkt fysiek op meerdere kanalen | N/A | ☐ |
| Dim-niveau werkt fysiek | N/A | ☐ |

## E2.3 — State sync: veldbus → Matter

| Check | Desk-lab | VLAN/cabinet |
|-------|----------|--------------|
| Serial: `Sync veldbus -> Matter: …` na statuswijziging | N/A | ☐ |
| Wandknop-schakeling zichtbaar in Apple Home (na poll/status) | N/A | ☐ |
| Status na herstart hub convergeert na eerste status-reply | N/A | ☐ |

**Let op:** relay/dimmer-modules melden status na een `S`/`C`-commando of statuswijziging — de poll-ack zelf bevat geen kanaalstatus (RE 2026-06-02). Externe schakelingen verschijnen dus zodra de module een statusbericht stuurt.

## E2.4 — Command path: Matter → veldbus

| Check | Desk-lab | VLAN/cabinet |
|-------|----------|--------------|
| Serial: `Matter -> veldbus: ep=… on_off=…` bij toggle | ☐ (TX zichtbaar) | ☐ |
| Toggle vanuit Apple Home schakelt fysiek | N/A | ☐ |
| Dim vanuit Apple Home zet fysiek niveau | N/A | ☐ |
| Geen command-echo-loop (sync-update triggert geen TX) | ☐ | ☐ |

## MVP-gates (beslist A3)

| Gate | Criteria | Status |
|------|----------|--------|
| **Apple Home** | Commissioning + on/off + dim | ☐ |
| **HA Matter** | Commissioning + on/off + dim | ☐ |

---

## Build / flash

```bash
cp config/devices.example.json config/devices.json   # of productie-export
idf.py build
idf.py -p /dev/cu.usbmodem* flash monitor
```

Commissioning-code verschijnt op serial bij eerste boot (of na factory reset: `idf.py erase-flash`).

---

## Serial log reference

| Direction | Example | Meaning |
|-----------|---------|---------|
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
- Dimmer "aan" zonder niveau (kaal on-commando) stuurt het laatst bekende niveau, anders 100%.
- Fan-endpoints gebruiken FanControl met sequence Off/High; Apple toont een ventilator-tegel.
- De echo-guard (`s_sync_in_progress`) voorkomt dat een veldbus→Matter sync opnieuw een veldbus-commando triggert.

---

## Verdict

| Gate | Criteria | Status |
|------|----------|--------|
| **Desk-lab E2 code** | Endpoints aangemaakt, commissioning start, geen crash | ☐ |
| **Apple Home MVP-gate** | Commissioning + on/off + dim fysiek | ☐ |
| **HA Matter MVP-gate** | Commissioning + on/off + dim fysiek | ☐ |
| **E2.3 state sync** | Externe schakeling zichtbaar in controller | ☐ |
| **Full E2** | Alle gates ☑ + evidence-document | ☐ |

**Evidence invullen:** `evidence/YYYY-MM-DD-e2-*.md` (patroon van E1-evidence volgen).
