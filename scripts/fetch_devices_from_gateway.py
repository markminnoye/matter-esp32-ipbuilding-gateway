#!/usr/bin/env python3
"""Fetch installation config from a running IPBuilding Gateway and write devices.json.

The gateway REST API does not expose the raw on-disk devices.json file.
This script reconstructs the embedded ``modules[]`` shape from:
  - GET /api/v1/modules  (module metadata: ip, mac, firmware, …)
  - GET /api/v1/devices  (per-channel config: ch, name, room, active, …)

Usage:
  python3 scripts/fetch_devices_from_gateway.py \\
      --host 3059e002-ipbuilding-gateway.local \\
      --port 8080 \\
      --out config/devices.json
"""

from __future__ import annotations

import argparse
import json
import sys
import urllib.error
import urllib.request
from collections import defaultdict
from typing import Any


def fetch_json(base_url: str, path: str, timeout: float) -> dict[str, Any]:
    url = f"{base_url.rstrip('/')}{path}"
    req = urllib.request.Request(url, headers={"Accept": "application/json"})
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return json.load(resp)


def devices_to_modules(
    modules_payload: dict[str, Any], devices_payload: dict[str, Any]
) -> dict[str, Any]:
    module_meta: dict[str, dict[str, Any]] = {}
    for mod in modules_payload.get("modules", []):
        ip = mod.get("ip")
        if not ip:
            continue
        module_meta[ip] = {
            "name": mod.get("name", ip),
            "ip": ip,
            "type": mod.get("type", "relay"),
            "firmware": mod.get("firmware", ""),
            "model": mod.get("model", ""),
            "mac": mod.get("mac", mod.get("id", "")),
            "channels": [],
        }

    channels_by_ip: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for dev in devices_payload.get("devices", []):
        device_type = dev.get("device_type")
        if device_type not in {"relay", "dimmer"}:
            continue
        module_ip = dev.get("module_ip")
        channel = dev.get("channel")
        if module_ip is None or channel is None:
            continue
        entry: dict[str, Any] = {
            "ch": channel,
            "name": dev.get("name", f"Ch {channel}"),
            "room": dev.get("room", ""),
            "active": dev.get("active", True),
        }
        semantic = dev.get("semantic_type")
        if semantic and semantic != "button":
            entry["semantic_type"] = semantic
        max_watt = dev.get("max_watt")
        if max_watt:
            entry["max_watt"] = max_watt
        channels_by_ip[module_ip].append(entry)

    modules: list[dict[str, Any]] = []
    seen_ips: set[str] = set()

    for ip, meta in sorted(module_meta.items(), key=lambda item: item[0]):
        mod = dict(meta)
        mod["channels"] = sorted(channels_by_ip.get(ip, []), key=lambda c: c["ch"])
        modules.append(mod)
        seen_ips.add(ip)

    for ip, channels in sorted(channels_by_ip.items()):
        if ip in seen_ips:
            continue
        modules.append(
            {
                "name": ip,
                "ip": ip,
                "type": channels[0].get("device_type", "relay") if channels else "relay",
                "firmware": "",
                "model": "",
                "mac": "",
                "channels": sorted(channels, key=lambda c: c["ch"]),
            }
        )

    return {"modules": modules}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="3059e002-ipbuilding-gateway.local")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--scheme", default="http", choices=["http", "https"])
    parser.add_argument("--out", default="config/devices.json")
    parser.add_argument("--timeout", type=float, default=10.0)
    args = parser.parse_args()

    base_url = f"{args.scheme}://{args.host}:{args.port}"

    try:
        status = fetch_json(base_url, "/api/v1/status", args.timeout)
        modules_payload = fetch_json(base_url, "/api/v1/modules", args.timeout)
        devices_payload = fetch_json(base_url, "/api/v1/devices", args.timeout)
    except urllib.error.URLError as exc:
        print(f"Failed to reach gateway at {base_url}: {exc}", file=sys.stderr)
        print(
            "Tip: use the gateway IP if .local mDNS does not resolve, e.g.\n"
            "  --host 192.168.1.x",
            file=sys.stderr,
        )
        return 1

    config = devices_to_modules(modules_payload, devices_payload)
    with open(args.out, "w", encoding="utf-8") as fh:
        json.dump(config, fh, indent=2, ensure_ascii=False)
        fh.write("\n")

    module_count = len(config["modules"])
    channel_count = sum(len(m["channels"]) for m in config["modules"])
    version = status.get("version", "?")
    print(f"Wrote {args.out} from {base_url} (gateway v{version})")
    print(f"  modules={module_count} channels={channel_count}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
