# RunClock v1.0.2-beta.3

Startup and input stability update for RunClock, the standalone PS5 speedrun timer by **BrewSparrow**.

## Changes

- Improved startup sequencing and retained the startup observer used in the successful console tests.
- Reduced resident integration to the two hooks needed for rendering and input.
- Removed automatic reinjection after preparation failure and kept resident dependencies loaded on rejection.
- Improved **L3 + R3** handling for held buttons, skipped samples and brief release bounces.
- Expanded local startup diagnostics and preserved previous launch logs.
- Updated the UI, ready notification, banners and release files to **v1.0.2-beta.3**.

## Testing

The underlying startup build passed **five consecutive launches**, alternating between **Restart** and a complete **Shutdown/reboot**, on the maintainer's PS5 running **firmware 13.60**. This release retains that startup path with updated version labels and packaging. Wider compatibility remains beta.

## Installation

1. Keep **`/data/RunClock/`** intact when updating and restart the PS5 before loading the new ELF.
2. Load **Y2JB Autoloader → Payload Manager → kstuff lite → elfldr → RunClock.elf**, waiting for each payload to finish. Both compatible **kstuff and elfldr are required before RunClock**.
3. Load RunClock once per boot. Wait for the ready notification, then hold **L3 + R3** to open it.

Players need **RunClock.elf** or the install ZIP containing the same ELF. No ProsperoMenu installation or external server is required. Matching application and dependency source archives accompany the release.

## Known limitations

- RunClock is still absent from PS5 built-in gameplay recordings; screenshot and recording support remain deferred.
- During a game, delete saved attempts using the menu's **Delete attempt → Confirm delete** controls. The double-Triangle shortcut is available from the home-screen panel.
- Saved attempts persist, but an active run does not recover after a crash, rest mode or reboot.
- Other firmware versions and simultaneous ShellUI overlays remain unverified.
