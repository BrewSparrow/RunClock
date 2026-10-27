# RunClock v1.0.0-beta.1

**Standalone PS5 speedrun timer by BrewSparrow.**

RunClock brings live timing, splits, and locally saved attempts to your PS5. Load **RunClock.elf** through Payload Manager. Everything runs on the console; ProsperoMenu and an external server are not required.

## Included

- Automatically updating active-time and real-elapsed clocks.
- Ten default splits and a separate **Finish run** control.
- Pause/resume, undo, and confirmed reset of both clocks.
- Local attempts, personal bests, and confirmed deletion.
- Timing while the panel is hidden; **L3 + R3** opens/closes it.
- **RunClock v1.0.0-beta.1** header with **BrewSparrow** beneath it.

## Setup

Tested by the maintainer on **PS5 firmware 13.60** through **Payload Manager**, using existing compatible **kstuff injection support**. Other firmware is unverified.

**Restart the PS5, load RunClock once, wait for its ready notification, and hold L3 + R3.** A clean restart restored in-game opening during testing. Keep other ShellUI overlay payloads disabled for the first boot test.

## Known issues

- **The RunClock overlay is absent from PS5 built-in gameplay recordings.** Capture controls were also reported unresponsive while the panel was open. Screen capture support is incomplete and deferred to a later update.
- **Use the Delete attempt / Confirm delete menu entries during a game.** The current game-input route does not forward the Triangle shortcut; double-Triangle deletion is available from the home-screen panel.
- Active attempts do not recover through a crash, rest mode, or reboot. Saved attempts persist.
- There is no always-visible gameplay timer HUD, autosplitting, or automatic load removal. Hardware timing latency has not been measured.

## Downloads

| File | Purpose |
|---|---|
| `RunClock.elf` | Payload for players |
| `RunClock_Standalone_Install_v1.0.0-beta.1.zip` | Same payload with instructions and notices |
| `RunClock_Standalone_Source_v1.0.0-beta.1.zip` | Modified application/engine source and exact build inputs |
| `RunClock_Dependency_Source_v1.0.0-beta.1.zip` | Corresponding native SDK/library source |
| `SHA256SUMS.txt` | SHA-256 checksums |
| `release-manifest.json` | Build identity, verification scope, and asset details |

The executable is unchanged from the maintainer-tested beta build. This finalization updates release documentation, packaging, and the asset verifier. Both source archives and the original notices accompany the binary under **GPL-3.0-or-later**. Rendering/input foundation: **StonedModder/Prospero-Menu**.

Saved data: **`/data/RunClock/`**. Bug reports should include firmware, Payload Manager/kernel setup, whether a game was running, reproduction steps, and the ELF checksum.
