# Context policy (embedded IPBuilding repo)

Doel: **minimaal tokens**, maximale signal. Algemene principes: *context quality > quantity*, progressive disclosure, subagent-isolatie.

## Vier buckets

| Bucket | Actie |
|--------|-------|
| **Write** | Feiten in repo (`resources_and_docs/`, sessienotities); niet alles in de chat herhalen. |
| **Select** | Alleen secties/files pullen die bij de vraag passen. |
| **Compress** | Tabellen > lange paragraaf; verwijs naar design spec i.p.v. kopiëren. |
| **Isolate** | Zware taken splitsen (veldtest vs firmware vs Matter commissioning). |

## Progressive disclosure (dit project)

| Tier | Wanneer | Voorbeelden |
|------|---------|-------------|
| **T0** | Altijd in scope | `AGENTS.md`, deze file |
| **T1** | Architectuur / implementatie | [embedded design spec](superpowers/specs/2026-07-09-embedded-ipbuilding-gateway-design.md) — relevante secties |
| **T2** | Veldtest / RE / protocol | `resources_and_docs/evidence/`, Python-repo [`RE_STATE.md`](https://github.com/markminnoye/IPBuilding-Gateway/blob/main/resources_and_docs/RE_STATE.md), [`gateway/payloads/`](https://github.com/markminnoye/IPBuilding-Gateway/tree/main/gateway/payloads) |
| **T3** | Zeldzaam / groot | PCAPs, volledige productie-`devices.json` — **nooit** hele bestand in context tenzij slice-analyse |

## Anti-patterns

- Hele design spec of Python `IPBUILDING_KNOWLEDGE.md` “voor context” laden.
- AGENTS.md-achtige tabellen dupliceren in antwoorden.
- Python northbound API-docs (`websocket.md`, `rest.md`) als embedded-contract behandelen — Gen 1 is Matter-only.

## Externe docs (Python-repo)

Veldbus-RE, capture-workflow en IPBox-referentie blijven in [`IPBuilding-Gateway`](https://github.com/markminnoye/IPBuilding-Gateway). Laad alleen wat nodig is voor payload-port of veldtest-correlatie.

## Repo-onderhoud

- **`AGENTS.md`:** kort houden; detail naar design spec + tier-T2 files.
- **Veldtest-evidence:** `resources_and_docs/evidence/` (deze repo) of verwijs naar Python-repo evidence bij gedeelde veldbus-tests.
