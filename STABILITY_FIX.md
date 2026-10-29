# RunClock startup and menu stability test

Current release: RunClock v1.0.2-beta.3, based on test 6 and five successful maintainer startup tests. See README.md, CONSOLE_TEST.md and VALIDATION.md; the changes below are historical.

RunClock v1.0.1-beta.2 by BrewSparrow. The changes below describe test 1 and remain included in startup-fault-diagnostic-test-6. The follow-up hardware test reached native phase 21 and lost the ShellUI target before readiness. See BOOTSTRAP_FIX.md for the code-buffer correction and RESTART_FIX.md for the earlier SDK-stage investigation and SYSCALL_FIX.md for the test-3 regression and STARTUP_DIAGNOSTIC.md for the current test-6 instructions.

## Findings

Further beta testing reported intermittent freezes/refused launches despite full restarts and shutdowns, plus a menu that opened and closed before it could be used. The first supplied log identifies the beta.2 binary and an unresolved earlier deck.start job. The follow-up runtime(3).log identifies test 1 and a separate failure during native bootstrap; test 1 is therefore not a verified startup fix. The current replacement still requires physical testing.

## Changes

- Check the recorded ShellUI process identity, not only whether its saved boot timestamp changed. An ended process can release its old reservation. A current or unverifiable owner remains protected from duplicate injection. Missing/superseded job receipts use the same read-only process check.
- Report a pending menu scene as alive while it activates. Previously its unready status could reset the game-input epoch during opening. Buttons still cannot activate rows until the actual frame is presented.
- Ignore skipped ShellUI controller samples instead of treating them as releases. Re-arm L3 + R3 after both sticks are released for at least 100 ms, preventing a held/bounced chord from repeatedly toggling the panel.
- Keep the current input route during an unknown foreground-focus observation. Verified focus changes and the existing capture lease expiry still release it.
- Give transient scene-root changes a bounded activation window; expiry closes cleanly so a later opening can retry.
- Preserve three older launch logs and log native owner classifications and input-relay resets. A subsequent retry no longer overwrites the only evidence of the first failure.

The UI version, developer credit, timer/storage worker, saved-record format, route format and capture limitations are retained. This is a test replacement, not a claim that the original intermittent freeze is fully resolved.

## Console test

1. Use this exact RunClock.elf. Fully restart the PS5 before switching builds, then load the usual jailbreak plus BOTH kstuff and elfldr. Wait for them to finish before launching RunClock once through Payload Manager. Keep other ShellUI overlays disabled for this test.
2. Wait for ready. Hold L3 + R3 until the panel opens, release both sticks, then navigate and start a run. Check repeated show/hide on the home screen and with a game running. Continuing to hold the chord should not close it again.
3. Test Split, Pause/Resume, Finish, Records and reset, including the existing saved attempts. No data deletion or extra runtime installation is needed.
4. After a further full restart, reload the required payloads and RunClock once, keeping the existing data. Check startup again.
5. If startup freezes or fails, retain /data/RunClock/runtime/runtime.log and deck-status.txt, plus runtime.previous.log if present before repeated attempts. runtime.2.log and runtime.3.log retain two additional launches. Note whether the failure happened during launch or when opening the panel, and whether a game was running.

Do not delete /data/RunClock. The logs cannot certify PS5 timing, native injection, or actual hardware stability; the automated tests substitute the console probes and do not emulate Mono/ShellUI.
