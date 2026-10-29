# RunClock native startup test 2

Current release: RunClock v1.0.2-beta.3, based on test 6 and five successful maintainer startup tests. See README.md, CONSOLE_TEST.md and VALIDATION.md; the changes below are historical.

RunClock v1.0.1-beta.2 by BrewSparrow. These are the historical test-2 changes, retained in startup-fault-diagnostic-test-6. A subsequent test loaded and opened well in a game but failed after a restart at SDK phase 203. See RESTART_FIX.md for the latest evidence and current instructions.

## Evidence from the latest test

The supplied runtime(3).log identifies source a8c048bfc7074fc62a5342b72d35452d5d0438dfe7eb8d965675bd9917f36fcd, the previous stability test. Startup issued a new injection and reached native phase 21. The target ShellUI process then became unavailable with errno 3 (ESRCH). deck-status.txt records state 0, render 0, input 0 and hooks 0. The coordinator reported DECK_UNCERTAIN. This failure precedes menu readiness; it is different from the earlier saved-owner refusal.

The old phase 21 spans trampoline preparation, optional dashboard/cheats preparation and code-buffer sealing. The log cannot distinguish those operations or prove which instruction caused the process to disappear. It also does not contain a working session demonstrating the L3 + R3 behavior.

## Correction

The resident allocated an anonymous writable trampoline buffer, then called the SDK kernel_mprotect helper directly to make it executable. SDK v0.43 edits protection on whole VM entries, without splitting at the requested range. When the allocation shares an entry with adjacent writable ShellUI memory, that operation can change the neighbors too. The outer injector already isolated its own code mapping, but the resident's separate buffer did not use that protection.

The resident now first uses native mprotect through the initialized libkernel gateway to separate the code buffer from adjacent writable mappings. Only after that succeeds does it request read/execute protection, verify the result, and proceed toward hook publication. A failed isolation or protection check rejects startup before publication. The resident helper also rejects writable/executable requests.

Startup now labels render/input trampoline preparation, optional-hook preparation and code-buffer sealing separately in runtime.log and deck-status.txt. The bootstrap record layout and protocol versions are unchanged. Test 1's menu-input, scene-readiness, owner-recovery and log-retention changes remain included.

## Verification and limits

The new host regression links the actual resident-memory.c with a bounded model of native range splitting and SDK whole-entry protection. The previous implementation fails because neighboring writable mappings change; the correction preserves them. Isolation failure, kernel-helper failure, verification failure, failed PID lookup and failed mapping are covered. These checks supplement the timing/storage, startup-recovery and native menu tests.

This is a concrete code defect and a candidate explanation for the intermittent startup failure. A host model cannot establish the cause of the recorded ShellUI disappearance or confirm PS5 stability. The exact new ELF still needs console testing. Timer/storage source, saved formats, UI title and developer credit are unchanged. Capture work remains deferred.

## Test on the PS5

1. Replace the ELF used by Payload Manager with this exact build. Fully restart before switching builds. Keep existing /data/RunClock data.
2. Load the usual jailbreak and BOTH kstuff and elfldr. Wait for them to finish, then load RunClock once. Keep other ShellUI overlays disabled for the test.
3. Wait for ready. Hold L3 + R3, release both sticks after opening, then test navigation, Start run and repeated show/hide. Check on the home screen and during a game.
4. Check Split, Pause/Resume, Finish, reset and Records. After another full restart, reload the prerequisites and RunClock once to check startup again.
5. If it fails, retain /data/RunClock/runtime/runtime.log and deck-status.txt. Include runtime.previous.log, runtime.2.log or runtime.3.log if present. A previous log exists only if an earlier runtime.log was present and successfully rotated; its absence alone does not identify the cause. Do not repeatedly reload into the failed session or delete ownership receipts.

Do not describe this test build as a confirmed freeze fix or a console-verified release until the exact ELF has passed hardware testing.
