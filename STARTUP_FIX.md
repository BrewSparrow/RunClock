# RunClock startup recovery build

**RunClock v1.0.1-beta.2 by BrewSparrow**

This release retains the earlier startup recovery changes. Further beta.2 testing found intermittent failures and menu closures, so the earlier working report does not certify general reliability. The process-owner and input/readiness changes are documented in STABILITY_FIX.md. The code-buffer correction is documented in BOOTSTRAP_FIX.md; the SDK handoff changes are documented in RESTART_FIX.md, and the test-3 bridge regression and test-4 correction are documented in SYSCALL_FIX.md. The UI remains RunClock v1.0.1-beta.2 with BrewSparrow beneath it. Physical testing of this new replacement is pending.

## Historical ownership-refusal log

The coordinator starts, then stops at the earlier-coordinator ownership check before issuing a new renderer injection. The log alone does not prove whether the previous hooks are still active. A missing ready heartbeat does not prove that hooks are gone.

## Changes

- Reconcile interrupted renderer and game-input startup jobs before starting a new renderer. Native boot/process checks must confirm that the old boot or game ended before releasing their reserved resources.
- Cancel queued startup intents that were never dispatched instead of replaying their old process identities.
- Recover a native stage-1 intent after its coordinator exits. Stage 1 is saved before the injector can begin; prepared, published and uncertain receipts remain protected.
- Retain the same-boot guard when an earlier renderer may still own hooks. The notification now explicitly asks for a PS5 restart.
- Include the error code and message in runtime.log even when QuickJS supplies a stack without its message.

Those historical recovery changes left timer/storage formats and capture behavior unchanged; current native changes are documented separately. The former release-guide file remains omitted from the source package.

## Install and test

1. Replace the RunClock.elf used by Payload Manager with this build. Do not use the previous ELF from another folder or automatic payload entry.
2. Fully restart the PS5. Rest mode does not unload resident hooks. Load the usual jailbreak and both kstuff and elfldr; wait for them to finish, keeping other ShellUI overlay payloads disabled for this test.
3. Load RunClock.elf once through Payload Manager. Wait for the ready notification, then hold L3 + R3.
4. Check Start run, Pause/Resume, Split, Finish run, Records and reset. Check that existing saved attempts are still present.
5. Fully restart again, then launch once more with the existing data still on disk. RunClock should recover any verified previous-boot startup reservations automatically.

Do not delete /data/RunClock to clear this error. Saved attempts and routes remain under that folder; runtime files are under /data/RunClock/runtime/.

If startup still fails, save /data/RunClock/runtime/runtime.log and deck-status.txt before further attempts. Include runtime.previous.log and other retained logs if present. Include whether this was the first launch after a full restart and the ELF SHA-256 from SHA256SUMS.txt.

## Verification and limits

The automated suite covers previous-boot reservations, queued intents, clean rejection, refusing a second coordinator in the same boot, transient and sustained probe contention, and startup error messages. It also runs the existing timing/storage worker and native control tests. These tests substitute console probes and injection; they do not emulate ShellUI or establish physical-console startup success.

The corresponding source and original license notices accompany the replacement. Existing screen capture limitations are deferred, as before. Test this exact ELF on the PS5 before publishing it as a replacement release.
