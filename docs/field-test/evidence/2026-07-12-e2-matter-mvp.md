# E2 Matter MVP — field test evidence (2026-07-12)

**Checklist:** [`../e2-matter-checklist.md`](../e2-matter-checklist.md)
**Faseroadmap (E0–E4):** [`../phases.md`](../phases.md) · [design spec §11](../../superpowers/specs/2026-07-09-embedded-ipbuilding-gateway-design.md#implementatiefasen)

---

## Scope-wijziging

**A3 herzien (2026-07-12):** Home Assistant Matter is geen verplichte MVP-gate meer. Apple Home is het enige controllerpad voor dit product; HA Matter blijft mogelijk als optionele testlaag maar bepaalt geen acceptatie. Zie design spec §14 (A3) en §8.2.

## Environment

| Item | Value |
|------|-------|
| Date | 2026-07-12 |
| Board | Waveshare ESP32-S3-ETH (ESP32-S3R8 + W5500) — ook bevestigd als productiehardware (H2) |
| Cabinet | Kunststof zekeringskast (H1: geen metalen afscherming, geen WiFi-attenuatie-risico) |
| Git branch / commit | `development` @ `9822fd2` |
| Controller | iPhone, Apple Home, thuis-LAN |

## E2.1 — Bridge: eerste relay endpoint

| Check | Result |
|-------|--------|
| Endpoints aangemaakt op boot, geen crash | ☑ |
| Endpoint↔kanaal mapping op serial | ☑ |
| Commissioning QR/pincode op serial console | ☑ |
| Apple Home commissioning slaagt | ☑ |
| Apple Home schakelt 1 relay-kanaal fysiek | ☑ |

## E2.2 — Alle actieve relay + dimmer endpoints

| Check | Result |
|-------|--------|
| Alle `active:true` kanalen zichtbaar in Apple Home | ☑ |
| Kanaalnamen (`NodeLabel`) zichtbaar als accessoirenaam | ☑ |
| Lichten als lamp, fans als ventilator, switch als stekker | ☑ |
| Dimmer toont aan/uit + helderheidsslider | ☑ |
| On/off + dim werken fysiek op meerdere kanalen | ☑ |

## E2.3 — State sync: veldbus → Matter

| Check | Result |
|-------|--------|
| Statuswijziging op de bus zichtbaar in Apple Home | ☑ |
| Boot sweep: relay-kanalen tonen juiste aan/uit-stand direct na commissioning, zonder fysieke toggle | ☑ |
| Dimmer-kanalen: "uit" tot eerste event — geaccepteerde native-parity limitatie, geen HTTP-workaround | ☑ |

## E2.4 — Command path: Matter → veldbus

| Check | Result |
|-------|--------|
| Toggle vanuit Apple Home schakelt fysiek | ☑ |
| Dim vanuit Apple Home zet fysiek niveau | ☑ |
| Dimmer "aan" na "uit" herstelt slider-niveau (niet 100%) | ☑ |
| Fan: toggle + speed-slider schakelen relay | ☑ |
| Geen command-echo-loop (sync-update triggert geen TX) | ☑ |

---

## Verdict

| Gate | Status |
|------|--------|
| **Desk-lab E2 code** | ☑ |
| **Apple Home MVP-gate** | ☑ |
| ~~HA Matter MVP-gate~~ | out of scope (A3 herzien) |
| **E2.3 state sync** | ☑ |
| **Full E2** | ☑ — **gesloten** |

**Summary:** E2 Matter MVP geaccepteerd op basis van fysieke commissioning en bediening in Apple Home (commissioning, on/off, dim, fan, en bidirectionele state sync inclusief boot sweep) zonder command-echo-loop. HA Matter is tijdens deze sessie uit scope gehaald (A3 herzien) en is dus geen blocker meer. H1 (WiFi-bereik) en H2 (productiehardware) zijn in dezelfde sessie afgesloten: de kast is kunststof (geen antenneprobleem) en het ESP32-S3-ETH devboard is bevestigd als productiehardware — geen apart DIN-rail board.

**Niet vastgelegd deze sessie:** serial log excerpt / screen recording. Toevoegen als die later beschikbaar is.
