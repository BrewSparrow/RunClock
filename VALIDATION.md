# Validation scope

## Automated host checks

python3 scripts/test.py drives the actual timer cores, native timing/storage worker, private loopback UI bridge, native input core and game-route core. It covers live clocks, ten splits, Finish, Pause/Resume, undo, reset, deletion and action/session/confirmation identity checks.

Startup tests drive the real state store, job queue and native adapters with substituted console probes. They cover changed boot identity, an ended ShellUI process with a repeated boot timestamp, superseded/missing native receipts, blocked current/unknown owners, native contention and undispatched intents. Native policy tests ensure unknown ownership cannot permit an uncertain retry.

New native stability tests use real encoded timer frames. They cover skipped samples, long-held/partially released/bounced L3 + R3 chords, deliberate reopening/closing, pending-scene readiness and action gating, unknown foreground observations, raw game-route capture acknowledgement and lease expiry. A temporary filesystem exercise checks that three prior startup logs are retained.

A native memory regression links the real resident-memory implementation with a model of coalesced VM entries. The old implementation fails by changing neighboring writable mappings; the corrected implementation preserves them. Failure paths reject sealing when isolation, kernel protection or verification fails.

These tests do not emulate Mono/ShellUI, perform a PS5 injection, reproduce an actual console freeze, certify game compatibility, or verify screenshot/video capture. A passing host test does not establish physical console stability.

Resident SDK tests validate fresh context, single-object checking and thread-only early rejection. A fixture executes the actual transformed SDK initializer with native boundaries substituted, confirming normal handle/firmware setup and rejection without the broad IOMMU scan.

The real SDK syscall bridge is also compiled with the resident toolchain/target and executed through an assembly mock gateway. The old frame-dependent code fails; the naked correction forwards all six arguments and returns safely. A byte-level guard validates the linked native bridge and repeats against the embedded release renderer. This catches a regression missed by the earlier mocked syscall tests.

Startup tests also assert that clean and retained preparation rejections each issue only one native start. The memory regression checks the actual isolation, execution-protection and verification diagnostics. The resident is compiled without the inherited dashboard/cheats hook modules; its main entry is declared non-returning and retains the SDK on rejection.

## Build and package checks

Test 6 exercises the actual diagnostic observer controller and x86 checkpoint. It verifies disarm-before-detach ordering, preserved faults/unexpected traps, timeout/fault races and retained signal delivery after a failed detach. SDK logging failure has its own thread-only rejection test. These are native host tests with substituted PS5 operations, not a PS5 tracing certification. An initial live integration run hit DECK_STALE; its standalone rerun and a full suite passed without timer/action changes.

Cross-compile with Zig 0.14.1 and PS5 Payload SDK v0.43. Verify ELF load-page separation, source fingerprint, embedded assets, the native timer/storage worker and unchanged timing source, archive CRCs/checksums and agreement between the direct ELF and install ZIP. Source and notices accompany the binary.

## Physical PS5 status

The maintainer reports five consecutive successful startup tests of the test-6 baseline, alternating Restart and full Shutdown/reboot on PS5 13.60, using Y2JB Autoloader, Payload Manager, kstuff lite and elfldr. The release retains that observer profile and native resident code/data. A section comparison confirms identical allocated resident sections; only debugger directory metadata differs. Other embedded native helpers are byte-identical. The new v1.0.2-beta.3 ELF changes version/display strings and release metadata; its exact hash has not yet been verified on the physical console. Build and automated checks are separate from hardware testing. Successful sessions were reported by the maintainer; no new runtime captures were supplied for independent inspection.

Other firmware, simultaneous ShellUI overlays, capture support, active-run recovery and hardware timing latency retain the limitations in KNOWN_ISSUES.md.
