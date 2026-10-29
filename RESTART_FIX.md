# RunClock resident SDK startup test 3

Current release: RunClock v1.0.2-beta.3, based on test 6 and five successful maintainer startup tests. See README.md, CONSOLE_TEST.md and VALIDATION.md; the changes below are historical.

RunClock v1.0.1-beta.2 by BrewSparrow. These are the historical test-3 changes, retained in startup-fault-diagnostic-test-6. Test 3 failed during launch; its rebuilt syscall bridge contained a stack-frame regression. See SYSCALL_FIX.md for that correction and the current test instructions. No successful test-3 session has been established.

## What the new files establish

Both logs identify source cda7b0658c954162b429e3b95a8b646b49fb45edc036eb72473447d4e077c103, test 2. The previous log shows successful native publication, ten hooks and repeated fresh renderer heartbeats. The maintainer reports that in-game L3 + R3 opening was substantially better and consistent in that session.

After a console restart, the current log identifies a new boot and ShellUI generation. The old receipt is correctly classified as owner=ended. A fresh injection begins. The ShellUI target then disappears during phase 203, inside the resident SDK kernel initializer, before Mono preparation or menu hook publication. The error is DECK_UNCERTAIN; deck-status records state 0 and hooks 0. The screenshots show CE-108262-9 and the RunClock startup error.

This is an earlier failure than test 1's phase-21 observation. The saved-owner reboot check is not blocking the new launch. Removing records is not established as a remedy. The logs locate the failure stage but contain no fault instruction or complete crash stack, so they cannot prove one exact cause.

## Candidate correction

Stock SDK v0.43 discovers the IOMMU object by walking kernel data and trying pointer-shaped values. The standalone coordinator has already completed that initialization before it can launch the resident renderer. Test 2 repeated the discovery inside ShellUI. The successful session recorded more than 36,000 kernel reads during resident initialization; the failed attempt disappears during that stage.

This build uses a resident-only CRT prepared from the exact bundled SDK v0.43 source. It replaces only IOMMU discovery with a fresh address handoff from the current coordinator. The coordinator validates the known object's device identifier before attaching to ShellUI. The resident validates the handoff's magic/version, firmware, data base and target PID, then checks that one object's identifier. Invalid or absent context is rejected; it never falls back to the broad pointer scan. No address is cached in a file or reused from a prior boot.

The SDK's normal handle setup, firmware table, process mappings and symbol initialization remain. The standalone coordinator and other helpers retain their stock SDK CRT. Original source/license notices remain included; engine/scripts/prepare-resident-crt.py records and checks the exact SDK source pin and applies the resident-only transformation during a build.

An early resident kernel or linker initialization error now publishes a clean rejection and exits that bootstrap thread through the initialized libkernel syscall gateway. It cannot fall through to the SDK process-termination path, which may trap when initialization is incomplete. Existing uncertain-hook protections remain. There is no automatic reinjection or receipt deletion.

The private bootstrap ABI is version 14 so mismatched native components fail validation. The timer wire protocol, saved records/routes, UI version and developer credit are unchanged. Test 1's input/readiness fixes and test 2's code-buffer isolation remain included. Capture work remains deferred.

## Validation

The tests exercise the real handoff validator and early-error wrapper with mocked syscall/kernel boundaries. They cover one bounded object read, incorrect firmware/data-base/PID/version, invalid addresses, wrong object identifiers and read errors. Early SDK/linker failures must report state 3 with no patches and request thread exit.

A second test runs the actual transformed SDK __kernel_init body with native boundaries substituted. It verifies handle and firmware setup, handoff use and rejection without a scan. The complete timer/storage, startup-recovery, input/route and code-buffer regression suite also runs. Cross-compilation checks native ELF mapping separation and embedded assets. These tests do not emulate the PS5 kernel or certify console stability.

## Required console test

1. Replace RunClock.elf with this exact build and fully restart. Keep /data/RunClock and its runtime folder intact. Back up saved attempts if desired; no folder deletion is required by this build.
2. Use the same tested sequence: Y2JB Autoloader, Payload Manager, kstuff lite and elfldr. Wait for both prerequisite payloads to finish before loading RunClock once. Keep other ShellUI overlay payloads disabled for this test.
3. Wait for ready. Check L3 + R3 opening, navigation and timer controls on the home screen and in a game. Release both sticks after opening.
4. Restart the console, leaving the RunClock data intact. Repeat the same prerequisite sequence and load RunClock once. Repeat across several boots if successful; one working session does not establish reboot reliability.
5. If startup fails, keep runtime.log, runtime.previous.log if present, and deck-status.txt from /data/RunClock/runtime. Save them before further attempts. The new log should identify fresh coordinator context before injection. Note whether CE-108262-9 appeared and whether failure occurred before ready or when opening the panel.

This is test 3, not a confirmed fix for every system-software crash. Do not publish it as hardware verified until this exact ELF passes repeated restart testing.
