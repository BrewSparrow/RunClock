# RunClock resident syscall correction — test 4

Current release: RunClock v1.0.2-beta.3, based on test 6 and five successful maintainer startup tests. See README.md, CONSOLE_TEST.md and VALIDATION.md; the changes below are historical.

RunClock v1.0.1-beta.2 by BrewSparrow. These are the historical test-4 changes, retained in startup-fault-diagnostic-test-6. Test 4 can launch but still fails intermittently during first-launch preparation. See MINIMAL_STARTUP.md for the paired test-4 logs and STARTUP_DIAGNOSTIC.md for current changes and instructions.

## Confirmed test-3 regression

runtime(5).log identifies test 3, source f7243ef451439861b4973d9ac786eedc403650c4b4590c8d65e8e162eec460f3. Old-boot ownership is correctly released and the coordinator verifies its fresh context. The target disappears during resident SDK initialization, before any kernel reads are recorded. The supplied previous log identifies test 2 (source cda7b065...), not a successful session of test 3.

Inspection of the compiled test-3 renderer found a concrete build regression. The rebuilt SDK __crt_syscall contains push rbp / mov rbp,rsp before its hand-written assembly. That assembly expects the original stack: it reads argument six at rsp+8 and ends with its own ret. With the added frame, that ret pops the saved frame pointer as a return address. The first firmware-query syscall can therefore return to an invalid address before any kernel-copy operation is recorded. The stock SDK bridge in test 2 has no such prologue.

This defect was introduced by the test-3 CRT rebuild. The earlier host tests substituted the syscall boundary and did not execute this assembly bridge. The correction below addresses this specific regression; it does not establish the cause of the earlier intermittent failures.

## Correction and build guard

engine/scripts/prepare-resident-crt.py now marks the SDK assembly bridge naked so the compiler cannot emit a C stack frame around it. The original SDK source archive and copyright/license notices remain unchanged. The resident IOMMU handoff, thread-only initialization rejection, input/readiness improvements and code-buffer isolation remain included.

The build verifies the actual linked resident ELF before embedding it. Its guard checks all register shifts, the sixth argument's original stack offset, the call to the bridge's writable gateway slot, and the direct return with no prologue. A build with the test-3 machine code is rejected. Release-asset verification repeats that check against the renderer actually embedded in RunClock.elf.

## Execution regression

tests/resident_syscall.py compiles the real SDK syscall source with Zig 0.14.1 and the resident target/options, including forced frame pointers. It links the object to an assembly mock gateway on the host. No operating-system syscall is issued by the mock. The uncorrected source fails in an isolated child process and fails the ELF guard. The corrected bridge executes repeated calls, preserves the frame pointer, forwards the syscall number and all six arguments, returns the expected value, and passes the guard.

This supplements the full timer/storage, startup recovery, input/route, code-buffer and resident-context regressions. The actual final native ELF must also pass the ABI guard. These checks verify the identified assembly regression, not PS5 injection, game compatibility or reboot reliability.

## Console test

1. Keep /data/RunClock and its runtime folder intact. Replace only RunClock.elf with this exact test-4 file, then fully restart the console.
2. Repeat Y2JB Autoloader → Payload Manager → kstuff lite → elfldr → RunClock. Wait for prerequisite payloads to finish and load RunClock once.
3. If ready appears, test L3 + R3, navigation and timer controls on the home screen and during a game. Then restart and repeat the launch sequence without deleting data.
4. If it fails, save runtime.log, runtime.previous.log if present, and deck-status.txt from /data/RunClock/runtime before further attempts. Note whether failure occurs before ready or when opening the panel.

The UI remains RunClock v1.0.1-beta.2 with BrewSparrow beneath it. The timing/storage worker and saved formats are unchanged. Screen capture remains deferred. Do not publish this as a confirmed solution to all startup crashes until the exact ELF passes repeated console tests.
