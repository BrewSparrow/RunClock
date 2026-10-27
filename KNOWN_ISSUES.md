# Known issues — v1.0.0-beta.1

This beta preserves the tested ELF. Screen capture changes are deferred.

## Built-in recording and screenshot controls

The maintainer reports that RunClock is visible over a running game but is **absent from PS5 built-in gameplay recordings**. RunClock currently draws through ShellUI. That design and the observed recording are consistent with the capture path omitting the system-interface overlay; a capture-compatible rendering path has not been verified.

Capture buttons were also reported unresponsive while the RunClock panel was open. The current controller filters suppress buttons while the panel owns input. The full screenshot/Create behavior after a clean restart has not been retested. Close the panel to use the normal system capture controls; this does not add RunClock to the recorded image.

This release does not claim screenshot or recording support for the overlay. A future update needs both input and image-capture testing on the console.

## Triangle shortcut during a game

Source inspection shows that the current raw game-input mapping does not forward Triangle. **Use Records → attempt → Delete attempt → Confirm delete during a game.** Double-Triangle deletion is available when using the home-screen input path. The release does not change the tested controller code.

## Opening the panel

The maintainer confirmed that restarting the PS5 restored opening RunClock while a game was running. Load RunClock once per boot. Reboot before switching builds or changing system-interface overlay payloads. Simultaneous ShellUI overlays are unverified.

## Other limits

- Firmware 13.60 through Payload Manager is the tested setup. Existing compatible kstuff injection support is required.
- Saved records persist, but active attempts do not recover after crash, rest mode, or reboot.
- No always-visible gameplay timer HUD, automatic load removal, autosplitting, or global split shortcut is included.
- Physical controller-to-timer latency has not been measured; displayed precision does not certify competitive timing accuracy.

See CONSOLE_TEST.md and VALIDATION.md for the distinction between maintainer observations, source inspection, and automated host checks.
