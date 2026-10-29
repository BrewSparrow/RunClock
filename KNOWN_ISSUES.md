# Known issues — v1.0.2-beta.3

This release retains the startup observer from test 6, which the maintainer reports passed five consecutive alternating Restart and complete Shutdown/reboot launches on PS5 13.60. Earlier builds crashed intermittently. Five successful tests establish progress on that setup, not universal compatibility or a proven sole root cause. The new version-label rebuild has a separate ELF hash; exact-artifact console verification is still pending. See CONSOLE_TEST.md and VALIDATION.md. Screen capture work remains deferred.

## Built-in recording and screenshot controls

The maintainer reports that RunClock is visible over a running game but is **absent from PS5 built-in gameplay recordings**. RunClock currently draws through ShellUI. That design and the observed recording are consistent with the capture path omitting the system-interface overlay; a capture-compatible rendering path has not been verified.

Capture buttons were also reported unresponsive while the RunClock panel was open. The current controller filters suppress buttons while the panel owns input. The full screenshot/Create behavior after a clean restart has not been retested. Close the panel to use the normal system capture controls; this does not add RunClock to the recorded image.

This release does not claim screenshot or recording support for the overlay. A future update needs both input and image-capture testing on the console.

## Triangle shortcut during a game

Source inspection shows that the current raw game-input mapping does not forward Triangle. **Use Records → attempt → Delete attempt → Confirm delete during a game.** Double-Triangle deletion is available when using the home-screen input path. The Triangle mapping is unchanged.

## Opening the panel

The maintainer confirmed that restarting the PS5 restored opening RunClock while a game was running. Load RunClock once per boot. Reboot before switching builds or changing system-interface overlay payloads. Simultaneous ShellUI overlays are unverified.

An earlier coordinator may leave native hooks resident after its process exits. This build automatically reconciles startup reservations only when native checks verify that the old boot or game ended, or that injection never began. A genuinely uncertain same-boot renderer still requires a full restart; deleting saved data is not a remedy.

## Other limits

- Firmware 13.60 through Payload Manager is the tested setup. Both kstuff and elfldr must be loaded before RunClock, with compatible injection support.
- Saved records persist, but active attempts do not recover after crash, rest mode, or reboot.
- No always-visible gameplay timer HUD, automatic load removal, autosplitting, or global split shortcut is included.
- Physical controller-to-timer latency has not been measured; displayed precision does not certify competitive timing accuracy.

See CONSOLE_TEST.md and VALIDATION.md for the distinction between maintainer observations, source inspection, and automated host checks.
