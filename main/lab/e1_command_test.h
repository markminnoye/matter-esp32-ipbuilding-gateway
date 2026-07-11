#pragma once

namespace ipbuilding::lab {

// E1.3 VLAN field test: gang relay (ch10) + bureau dimmer (ch1).
// Accepted 2026-07-11 — not wired in app_main by default.
// To re-run: call from app_main after veldbus::start() (~10s eth link-up).
void run_e1_command_test();

}  // namespace ipbuilding::lab
