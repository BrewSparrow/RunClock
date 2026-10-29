<p align="center"><img src="assets/runclock-banner.svg" alt="RunClock standalone PS5 speedrun timer" width="960"></p>

# RunClock v1.0.2-beta.3

**By BrewSparrow**

Time your runs, record splits, and save attempts directly on your PS5. RunClock is a standalone **RunClock.elf** payload loaded through **Payload Manager**. It runs entirely on the console and requires no external server, account, PC companion, or ProsperoMenu installation.

**Beta 3 · RunClock v1.0.2-beta.3.** Based on the startup build that passed five consecutive alternating Restart and full Shutdown/reboot tests on the maintainer's PS5 13.60. The tested startup observer remains enabled. See [Changelog](CHANGELOG.md), [console test history](CONSOLE_TEST.md) and [Known issues](KNOWN_ISSUES.md).

## Features

- Live active-time and real-elapsed clocks update when you select **Start run**.
- Ten default splits, with a separate **Finish run** control.
- Pause/resume active time while real elapsed continues.
- Undo the latest split and reset both clocks with confirmation.
- Save attempts and personal bests locally, browse records, and delete saved attempts with confirmation.
- Keep timing while the panel is hidden; hold **L3 + R3** to show or hide it.
- Use a custom route with 1–32 checkpoints.

## Download and install

Open this repository's **Releases** section and select **v1.0.2-beta.3**. Players need **RunClock.elf**, or the install ZIP containing the same ELF plus instructions and notices.

1. **Restart your PS5 before loading this build.** Load RunClock once per boot. Start with other ShellUI overlays, including ProsperoMenu, disabled for that boot.
2. Start your usual jailbreak and ensure **both kstuff and elfldr are loaded and have finished starting** before loading **RunClock.elf** through Payload Manager. Loading RunClock without them has been reported to freeze the console.
3. Wait for the ready notification, then hold **L3 + R3** to open the panel.
4. Select **Start run**. Both clocks update automatically. The panel can be opened from the home screen or during a game on the tested setup.

**Requirements:** a jailbroken PS5 with **kstuff and elfldr loaded before RunClock** and compatible injection support. The tested setup uses firmware **13.60** and Payload Manager. RunClock supplies its own renderer and timing worker. It does not install kernel support or replace another application's ELF. Other firmware and simultaneous system-UI overlays are unverified.

If the panel opens on the home screen but fails to open in a game, restart the console and load RunClock once. This restored in-game opening during the maintainer's testing.

## Controls

| Control | Action |
|---|---|
| L3 + R3, held together | Show/hide RunClock |
| D-pad / Cross | Navigate/select |
| Start run | Start both clocks at zero |
| Split | Record the next checkpoint; split 10 does not finish the run |
| Finish run | Freeze both clocks and save the recorded checkpoints; works while running or paused |
| Pause active timer / Resume | Pause/resume active time while real elapsed continues |
| Undo last split | Remove the latest unfinished checkpoint |
| Reset clocks → Confirm reset clocks | Clear both clocks; save an unfinished attempt as reset |
| Records → attempt → Delete attempt → Confirm delete | Delete that saved result and recalculate the personal best |
| Triangle, then a second Triangle press in saved-attempt details | Open/confirm deletion from the home-screen panel; use the menu controls during a game |
| Circle | Go back, cancel a confirmation, or close the main panel |

Finish can save a run with zero, some, or all splits recorded. Unused splits stay `--`. Holding a button cannot confirm deletion. Resetting the clocks keeps completed records.

## Saved data and custom routes

Records are stored in **`/data/RunClock/`** on the PS5. Runtime files are created automatically in **`/data/RunClock/runtime/`**; no separate worker installation is needed.

For a custom route, create **`/data/RunClock/route.txt`** with the game title on the first line, category on the second, then 1–32 checkpoint names on separate lines. Restart the console/app after editing it. Changing route text gives it a different record identity; existing history remains on disk.

## Beta status

The startup build used for this release passed five consecutive maintainer tests, alternating between Restart and a complete Shutdown/reboot. The tested sequence was Y2JB Autoloader → Payload Manager → kstuff lite → elfldr → RunClock on PS5 firmware 13.60. Version 1.0.2-beta.3 retains that startup observer and the existing timer/input behavior; its version labels and release packaging have been rebuilt.

This is still a beta, and the test report applies to that setup and baseline. The versioned ELF is a new artifact; give it a final launch check before publishing or replacing a known working copy. Keep existing data. If startup fails, save runtime.log, deck-status.txt and bootstrap-stack.bin if present from /data/RunClock/runtime/ before retrying. Restart before loading another copy.

**RunClock is absent from PS5 built-in gameplay recordings.** Capture support remains incomplete and deferred.

An active attempt does not recover after a crash, rest mode, or reboot. Saved attempts persist. Automatic load removal, autosplitting, global split shortcuts, and an always-visible gameplay timer HUD are not included. Controller-to-timer latency has not been measured on hardware.

See [Known issues](KNOWN_ISSUES.md), [console test history](CONSOLE_TEST.md), and [validation scope](VALIDATION.md). Report problems using the included bug-report template, with firmware, reproduction steps, and the ELF checksum.

## Source, credits, and license

RunClock is **GPL-3.0-or-later**. Its rendering/input foundation is adapted from [StonedModder/Prospero-Menu](https://github.com/StonedModder/Prospero-Menu), revision `84dd26cd342a5d9b1df5a93942ca5d7f10c8d39e`. Original copyright and license notices are retained. RunClock runs independently of that application.

Both **RunClock_Standalone_Source_v1.0.2-beta.3.zip** and **RunClock_Dependency_Source_v1.0.2-beta.3.zip** accompany the binary. See [build instructions](BUILD.md), [third-party notices](THIRD_PARTY.md), and [startup recovery instructions](STARTUP_FIX.md).
