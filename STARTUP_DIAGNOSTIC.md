# RunClock startup observer — test 6 history

RunClock v1.0.1-beta.2 by BrewSparrow. Internal revision: startup-fault-diagnostic-test-6. This page records the test-6 diagnostic implementation. The maintainer later reported five consecutive successful restart/shutdown launches. RunClock v1.0.2-beta.3 retains this observer; consult README.md, CONSOLE_TEST.md and VALIDATION.md for current release status.

## Latest console evidence

The maintainer reports two successful restart tests with test 5, followed by a crash on the third. The supplied runtime(7).log identifies source 4a6fb6be0a589ecc79bfd9baf3dc66e1f1b68a010f453d1e691ed4951ae41f70. It verifies a fresh coordinator SDK seed for original ShellUI PID 59, observes 739 kernel reads, and last observes phase 204 (resident SDK kernel initialization returned). That target then disappears with ESRCH, and the coordinator reports DECK_UNCERTAIN without another injection. deck-status(4).txt agrees: state 0, phase 204, zero hooks and memoryStage 0.

Phase 204 is the last sampled stage, not a captured fault instruction. Preparation can advance between the old 100 ms samples. These files establish that the coordinator did not publish the menu hooks; they do not establish the faulting instruction, thread, signal or exact cause. Two successful launches did not establish restart reliability. There is no evidence here that deleting records or changing the payload order would fix it.

## Diagnostic scope

- Watch the original ShellUI process from a checked bootstrap handshake through complete renderer/input preparation, before publishing either hook.
- Wait for the parent before the resident's first diagnostic firmware query. Use separate handshake fields; never use the hook-count field for tracing.
- Stop at an exact, address-checked breakpoint only after preparation reaches phase 23, or after an explicit preparation rejection. The older private diagnostic stopped at phase 204 and would miss a later fault; that endpoint is removed.
- On the first observed signal, capture the reporting thread, signal/code, fault address, registers, instruction bytes, bootstrap stage, memory protections and up to 8 KiB of its stack. Record the resident load bias for interpretation against the matching ELF.
- Disable the checkpoint before detaching, including on a fault or timeout. Preserve real signals; suppress only the exact owned checkpoint and an owned timeout stop. A fault racing the timeout keeps its real signal. A failed detach retains that signal for cleanup. A checkpoint that cannot be disabled is not detached blindly.
- Write diagnostics only after confirmed detach. A successful checkpoint then allows the existing hook-publication flow to proceed. An unrecognized first signal ends this launch's diagnostic attempt; it does not publish hooks or start another image.
- Add separate phases for logging initialization and the existing libc thread-flag lookup/write between SDK kernel setup and the resident linker. Record logging setup failure with thread-only early rejection.

A first signal may be part of Mono's own signal handling, rather than the final fatal event. Such a capture is evidence to interpret, not automatic proof of the root cause. Observation also changes startup scheduling, so successful diagnostic launches alone cannot certify the ordinary build. An observation timeout can stop this diagnostic attempt even if normal initialization would eventually finish.

The two essential hooks, one-attempt policy, rejected-resident ownership, syscall bridge correction, input handling and code-buffer isolation remain. No timer/storage source, record format, route format, version label or BrewSparrow credit changes. The private bootstrap ABI is 16; the timer wire protocol is still 14. Screen-capture work remains deferred.

## Console test

1. Keep /data/RunClock/ intact. Replace only RunClock.elf, then fully restart the PS5.
2. Use Y2JB Autoloader, then Payload Manager to load compatible kstuff lite and elfldr. Wait for each to finish, then launch RunClock.elf once. Keep other ShellUI overlays disabled for that boot.
3. If ready, check L3 + R3 and Start run. Restart and repeat the same sequence, keeping existing data. Record how many launches succeeded before any failure.
4. At the first failed launch or diagnostic rejection, stop testing and copy these files before restarting or attempting RunClock again:
   - /data/RunClock/runtime/runtime.log
   - /data/RunClock/runtime/deck-status.txt
   - /data/RunClock/runtime/bootstrap-stack.bin, if present
   - runtime.previous.log, if present
5. Restart before another attempt. Do not delete records or repeatedly load copies into the same boot. Keep a successful diagnostic runtime.log too, if all attempts succeed.

The runtime log should contain `RunClock v1.0.2-beta.3 startup observer` (test 6 used `RunClock startup diagnostic: test 6`) and `RunClock bootstrap first stop`. `checkpoint=1` at phase 23 means preparation reached the intended endpoint; it does not itself prove the renderer became ready. `checkpoint=0` means the signal/timeout needs review. A stack capture is rotated to bootstrap-stack.previous.bin at the next injection; save the current capture with its current log before another attempt.

## Verification limits

The host suite exercises the production observer controller for normal checkpoints, real faults, unexpected traps, timeout races, failed disarm and failed detach. It also executes the actual x86 checkpoint, verifies its recorded instruction address and checks that disarming prevents another trap. Existing kernel-seed, transformed SDK initializer, naked syscall bridge, timing, storage, input, route and startup-ownership regressions remain.

An initial live loopback integration run rejected a stale action with DECK_STALE. An independent integration rerun and the complete suite passed without changing timer or coordinator action logic. This is retained as a host-test limitation; it is not presented as a console crash reproduction.

Compilation and host tests cannot emulate PS5 ptrace/Mono behavior or establish first-launch reliability. The test-6 baseline subsequently passed five consecutive maintainer startup tests. The new v1.0.2-beta.3 version rebuild has a different ELF hash and requires its own final launch check; the baseline report is not an exact-artifact certification.
