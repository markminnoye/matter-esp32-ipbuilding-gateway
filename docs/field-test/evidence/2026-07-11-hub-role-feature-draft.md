# Hub_role / per-module managed — feature draft

**Status:** filed as [IPBuilding-Gateway #16](https://github.com/markminnoye/IPBuilding-Gateway/issues/16)  
**Source:** embedded E1.5 field test 2026-07-11 (evidence only — implementation priority is Python)  
**Priority:** **Spoor B — Python gateway first**; embedded Matter hub (Spoor C) mirrors later

---

## Problem

Today the Python gateway polls **all** modules in `installation.modules`, including input (IP1100). That claims the input module as centrale and receives `B-…E` events.

Operators want (especially on **Plan B / Pi standalone Python**):

- **Relay/dimmer** control via the gateway
- **Inputs not taken over** — autonomous (EEPROM) or delegated to another centrale

## Evidence (embedded field test — hypothesis validation)

| Scenario | Input poll | Button events to hub |
|----------|------------|------------------------|
| Hub does not poll `.50` | ✗ | ✗ (buttons still work locally) |
| Hub polls `.50` | ✓ | ✓ `B-…E` press/release |

See: [`2026-07-11-e1-input-listener.md`](2026-07-11-e1-input-listener.md)

**Hypothesis:** IP1100 slave-mode registration is poll-gated.

## Proposed config

### Per-module `managed` (recommended)

```json
{
  "modules": [
    { "ip": "10.10.1.30", "type": "relay", "managed": true },
    { "ip": "10.10.1.40", "type": "dimmer", "managed": true },
    { "ip": "10.10.1.50", "type": "input", "managed": false }
  ]
}
```

### Optional preset: `hub_role`

`full` (default) | `actuators_only` — sugar for all inputs `managed: false`.

## Implementation priority

| Phase | Product | Repo |
|-------|---------|------|
| **1** | Python gateway (Spoor B) | [`IPBuilding-Gateway`](https://github.com/markminnoye/IPBuilding-Gateway) — **#16** |
| **2** | Embedded Matter hub (Spoor C) | `matter-esp32-ipbuilding-gateway` — after Python ships + Spoor B field validation |

## Python gateway scope (phase 1)

| Area | Change |
|------|--------|
| `udp_bus.py` poll loop | Only poll `managed != false` |
| Input listener | Skip `B-…E` for unmanaged modules |
| `devices.json` schema | Optional `managed` (default `true`) |
| REST `/api/v1/status` | Expose hub role / managed flags |
| Docs | Actuator-only migration guide |

## Embedded firmware (phase 2 — deferred)

Same `managed` flag in LittleFS `devices.json`; input poll only when `managed: true`. Not blocking E2 Matter MVP.

## Acceptance criteria

1. `managed: false` on input → no `I0000` TX (logs + PCAP)
2. Physical buttons still actuate loads (EEPROM)
3. No `B-…E` on WS/REST when input unmanaged
4. `managed: true` → unchanged behaviour
5. Relay/dimmer `managed: false` → excluded from poll

## Open questions

- [ ] Dual-poller / last-poller-wins
- [ ] PCAP: centrale IP learning
- [ ] Unmanaged inputs in `/api/v1/devices`?
- [ ] HA companion warning?

## Related

- [IPBuilding-Gateway #16](https://github.com/markminnoye/IPBuilding-Gateway/issues/16)
- Embedded design spec §4.3
- RE: Sprint 5, `IPBUILDING_KNOWLEDGE.md` §6.4
