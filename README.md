# matter-esp32-ipbuilding-gateway

Standalone **ESP32-S3** firmware: IPBuilding veldbus-hub (UDP/1001) with **Matter** northbound.

Replaces the proprietary IPBox hub role on the field bus and exports relay/dimmer channels to Apple Home and Home Assistant Matter — without a Python gateway on the board (Gen 1).

## Deployment variant

| Variant | Platform | Status |
|---------|----------|--------|
| **Embedded Matter hub** (this repo) | ESP32-S3-ETH (W5500 + WiFi) | E1 veldbus MVP — payloads, UDP poll, LittleFS config |
| HA add-on + companion | Python gateway on HA | [IPBuilding-Gateway](https://github.com/markminnoye/IPBuilding-Gateway) — shipping |
| Standalone Python (Pi 3B) | Linux | Parallel validation track |

## Architecture

Canonical design: [`docs/superpowers/specs/2026-07-09-embedded-ipbuilding-gateway-design.md`](docs/superpowers/specs/2026-07-09-embedded-ipbuilding-gateway-design.md)

Agent brief: [`AGENTS.md`](AGENTS.md)

## Network (Gen 1)

```
ESP32-S3-ETH
├── eth (W5500)  → 10.10.1.1     UDP/1001  (IPBuilding VLAN)
└── wlan         → 192.168.x.x    Matter over WiFi (home LAN)
```

## Development setup (macOS)

### 1. Install ESP-IDF v5.5.4 (EIM)

Recommended for Matter development (esp-matter maintainer guidance, v5.5.x line):

```bash
brew tap espressif/eim && brew trust espressif/eim && brew install eim
eim install -i v5.5.4 -t esp32s3 --idf-features=mcp -p ~/.espressif
eim select v5.5.4
source ~/.espressif/tools/activate_idf_v5.5.4.sh
```

Use `eim run "idf.py build"` when the environment is not activated (e.g. Cursor MCP). Add the activation script to your shell profile, or run it in each terminal session before building.

### 2. Configure WiFi credentials

Matter commissioning uses WiFi on the home LAN. Set credentials via menuconfig (never commit `sdkconfig` with real passwords):

```bash
cd matter-esp32-ipbuilding-gateway
idf.py set-target esp32s3
idf.py menuconfig
# → IPBuilding Gateway → WiFi SSID / WiFi password
```

Ethernet field-bus IP defaults to `10.10.1.1` (same menu).

### 3. Build, flash, monitor

```bash
idf.py build
idf.py -p /dev/cu.usbmodem* flash monitor
```

**Note:** esp-matter is pulled automatically via the Component Registry (`main/idf_component.yml`). No separate esp-matter clone is required.

### 4. Flash devices.json (E1)

LittleFS stores field-bus config. Before first flash:

```bash
cp config/examples/devices.example.json config/devices.json
```

The build embeds **only** `config/devices.json` into the LittleFS partition (not the example template). Edit `config/devices.json` for your installation (gitignored).

### 5. Payload unit tests (optional)

```bash
cd test/payload
idf.py build
```

## Related repos

| Repo | Role |
|------|------|
| [IPBuilding-Gateway](https://github.com/markminnoye/IPBuilding-Gateway) | Python gateway + `gateway/payloads/` protocol reference |
| [ha-ipbuilding-gateway](https://github.com/markminnoye/ha-ipbuilding-gateway) | HA companion (Spoor A/B) |

## Documentation

- [`docs/README.md`](docs/README.md) — spec index
- [`docs/field-test/e0-bootstrap-checklist.md`](docs/field-test/e0-bootstrap-checklist.md) — E0 verification steps
- [`docs/field-test/e1-veldbus-checklist.md`](docs/field-test/e1-veldbus-checklist.md) — E1 verification steps
- [`config/examples/devices.example.json`](config/examples/devices.example.json) — config template (not flashed)

## Firmware layout

```
main/
├── app_main.cpp
├── board/          # Waveshare ESP32-S3-ETH (W5500)
├── network/        # Dual-netif: ETH + WiFi
├── matter/         # esp-matter bridge (E0 placeholder endpoint)
├── config/         # devices.json on LittleFS (E1)
├── registry/       # device registry (E1)
└── veldbus/        # UDP bus + payloads/ (E1)
```

## License

See [LICENSE](LICENSE).
