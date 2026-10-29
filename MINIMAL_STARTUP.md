# RunClock minimal startup test 5 — history

RunClock v1.0.1-beta.2 by BrewSparrow. These are historical test-5 changes, retained in diagnostic test 6. Test 5 passed two restart tests and crashed on the third; runtime(7).log last sampled phase 204 with no published hooks. It is not stable across restarts. Follow STARTUP_DIAGNOSTIC.md for the current build and instructions.

## Evidence from the five supplied files

All four runtime logs identify test 4, source 7d145cdd85f85b5d68a86e2f8abcde3d935726fab78036100e7f994996e2be24. The original ShellUI process (PID 59) fails during one launch at phase 312, which prepares inherited dashboard trampolines. In another boot it explicitly rejects code-buffer protection at phase 340, error 3, before publishing hooks. The coordinator labels that rejection safe and automatically starts another resident image. ShellUI then disappears during the second attempt.

The successful manual retries attach to replacement ShellUI processes (PID 103 and PID 102 respectively) within those same boot identities. They publish ten hooks and maintain fresh heartbeats. This establishes that retry success follows a system-interface process replacement. It does not establish why the first process failed, nor which protection operation returned an error. The supplied deck-status file belongs to the successful retry.

## Targeted changes

- Prepare and publish only the two hooks RunClock needs: rendering and input. Inherited dashboard decoration, Options/cheats actions and version-decoration hook modules are omitted from the resident build. Their preparation and tick/poll callbacks are removed. Timer controls and the game-input relay remain.
- Issue one native preparation per launch. Pre-injection readiness checks can still wait, but a preparation rejection no longer starts another resident image automatically.
- A resident main preparation failure now records rejection and stays resident with its worker disabled. It cannot return through the SDK's dependency-finalization path. Its imported dependencies and image remain owned until ShellUI exits.
- Preserve ownership after a resident rejection, even when no hooks were published. No patches does not mean no SDK/Mono state was created. Another attempt against that live owner requires a restart. Previous-boot or ended-process recovery remains.
- Record which code-protection step failed: native range isolation, SDK execution protection or final verification. runtime.log and deck-status.txt include the operation results, observed protection and errno. No allocation-layout or permission-policy workaround is applied without those results.

The private bootstrap ABI is version 15. Timer frame protocol, active/real-time logic, saved records/routes, UI version and BrewSparrow credit are unchanged. The test-4 naked syscall bridge and final-ELF check remain. Capture work remains deferred.

These changes target observed failure paths and remove unnecessary startup work. The logs do not prove that optional-hook preparation, SDK finalization or the automatic retry is the sole cause of every crash. The exact reason for the permission rejection still requires console evidence.

## Verification

Host tests drive the real job queue and native adapter to ensure one native start after either a clean rejection or a retained/uncertain rejection. Native memory tests check isolation/sealing/verification diagnostics alongside the existing neighbor-protection checks. The full timer, storage, input, route, startup recovery and actual syscall-bridge execution regressions run as before. The final ELF is checked for the correct syscall stack ABI and the absence of the removed hook modules. The timer/storage worker remains byte-identical to test 4.

These are host and compile checks, not PS5 kernel/Mono emulation or a console stability certification.

## Console test

1. Keep /data/RunClock and its runtime folder intact. Replace only RunClock.elf with this exact build, then fully restart.
2. Repeat Y2JB Autoloader → Payload Manager → kstuff lite → elfldr → RunClock. Wait for each prerequisite to finish. Load RunClock once.
3. If ready appears, test L3 + R3 and normal controls on the home screen and during a game. Then restart or shut down and repeat without deleting data.
4. If preparation fails, save runtime.log and deck-status.txt from /data/RunClock/runtime before another attempt. Include runtime.previous.log if present. The new code-protection fields are the useful evidence if phase 340 fails again.
5. This build deliberately stops after a resident preparation failure. Restart before another attempt; do not expect the old automatic retry or repeatedly load copies in the same live ShellUI process.

Do not publish this as a confirmed first-launch fix until the exact ELF has passed repeated console tests.
