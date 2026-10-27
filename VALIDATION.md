# Validation scope

## Automated host checks

`python3 scripts/test.py` exercises the C and JavaScript timer cores and the real C timing/storage worker. Integration coverage includes live active/RTA redraws, delayed Pause/Resume while digits change, ten checkpoints, independent Finish, resetting both clocks, duplicate button requests, stale page/session/confirmation protection, personal bests, deletion failure handling, and hidden timing.

Real frames from that test feed the adapted native C input core. The checks cover clock redraws preserving action identity, held-button suppression, Triangle deletion/confirmation using the managed input constants, and main-page Circle closing. They also check authenticated loopback frames and rejected requests.

These tests **do not emulate ShellUI, the full raw in-game controller route, or PS5 screenshot/recording behavior**. They do not verify end-to-end game-input delivery or hardware timing latency. Managed Triangle tests do not establish that the raw game-input mapping forwards Triangle; it currently does not.

## Binary and archive checks

The supplied ELF was cross-compiled during development. Final release preparation keeps that exact executable. The release verifier checks its architecture, identity, embedded timing worker and native assets, archive CRCs, file checksums, source availability, and agreement between the direct ELF and the install ZIP.

The coordinator uses private loopback addresses on ports 8890 and 8891. Runtime files are under `/data/RunClock/runtime/`; user data is under `/data/RunClock/`. No external server is needed at runtime.

## Physical PS5 observations

The maintainer reported working timer/storage controls on firmware 13.60 through Payload Manager and subsequently confirmed in-game opening after a restart. Capture controls with the panel open were reported unresponsive, and gameplay recordings omit RunClock. These are known beta limitations. CONSOLE_TEST.md and release-verification.json record the exact tested ELF and observation scope.

Other firmware, simultaneous ShellUI injectors, and active-attempt recovery through crash/rest/reboot remain unverified or unsupported. Displayed timer precision does not certify competitive timing accuracy.
