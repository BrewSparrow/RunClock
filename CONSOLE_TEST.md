# Console verification history

## Current release candidate: RunClock v1.0.2-beta.3

Revision v1.0.2-beta.3-release. The current ELF is rebuilt for version labels, the ready notification and release packaging. Its resident startup/render/input module is retained from the tested baseline. Compile and host checks are recorded in release-verification.json. The exact versioned ELF has not yet been reported tested on the PS5.

## Test 6: five consecutive successful startup tests

Baseline ELF SHA-256 702d02eeb7729d4838e4afea64dfa95bced95ec396d8bd8118f64eb4e6097340; source 20c2b94645b71ab8a492e662adee2c21395effa397db2f75e827f262df06dff7. The maintainer reports five consecutive successful launches, alternating Restart and complete Shutdown/reboot, with no issue. Setup: PS5 13.60, Y2JB Autoloader → Payload Manager → kstuff lite → elfldr → RunClock. No runtime logs from those successful sessions were supplied, so the exact signal/checkpoint behavior was not independently inspected. This report supports the retained startup profile; it does not prove a sole cause for earlier crashes or certify all setups.

## Stability test 5 restart failure

ELF SHA-256 0825edd0248612b4346ecc88b9b468d16b6352641fbaadf853d65c6b6bf6cba7; source 4a6fb6be0a589ecc79bfd9baf3dc66e1f1b68a010f453d1e691ed4951ae41f70. The maintainer reports two successful restart tests followed by a crash on the third. runtime(7).log verifies a fresh SDK seed, observes 739 kernel reads and last samples phase 204 before original ShellUI PID 59 disappears. deck-status(4).txt reports state 0, phase 204, hooks 0 and memoryStage 0. No automatic reinjection occurs. The exact fault instruction is not captured, and stages may advance between samples.

## Stability test 4 paired failures/retries

ELF SHA-256 bd34c0bb02ce82bf6118f8f048445483d4a37c2ecb2006166aad82ae454b7e9f; source 7d145cdd85f85b5d68a86e2f8abcde3d935726fab78036100e7f994996e2be24. The maintainer reports repeated restart/shutdown tests: launch sometimes fails, while reopening Payload Manager and loading again succeeds. runtime.3.log loses original ShellUI PID 59 during inherited dashboard trampoline preparation (phase 312). runtime.previous.log records phase-340 protection rejection, an automatic retry and then target loss. Successful logs attach to replacement PID 103 or 102 within those same boot identities and maintain healthy heartbeats. The precise fault instruction and protection sub-operation are not captured. The successful deck-status file does not describe the preceding failed attempt.

## Stability test 3 failure

ELF SHA-256 4d3714a0e8981038d2141c8dd9d96e0bc4cbc8a17d3b9d0833fae28ff6e5b96c; source f7243ef451439861b4973d9ac786eedc403650c4b4590c8d65e8e162eec460f3. The maintainer reports another launch crash after a complete restart and the required payload sequence. runtime(5).log reaches SDK phase 203 with verified coordinator context before ShellUI disappears; no kernel reads are recorded. Its previous log belongs to test 2. Inspection and an execution regression confirm that test 3's rebuilt bridge has an invalid C prologue; this was introduced by the CRT rebuild. No successful test-3 session has been established.

## Stability test 2 observations

ELF SHA-256 18764ad7c7c97117c6aa924a44c51d200a1930e3b86e80fcf9c13bb6933d3489; source cda7b0658c954162b429e3b95a8b646b49fb45edc036eb72473447d4e077c103. After a clean installation, the maintainer reports successful loading and substantially improved, consistent in-game L3 + R3 opening. runtime.previous.log confirms publication and fresh heartbeats. After restarting, the old receipt is correctly classified as ended; a new injection loses ShellUI at phase 203 (resident SDK initialization). The current log reports DECK_UNCERTAIN, and screenshots show CE-108262-9. One successful session does not establish reboot reliability.

## Stability test 1 observation

ELF SHA-256 61c50665d8a0a4cada955e9f85d44ae5584595b1795436fbb85305849e838ca7; source a8c048bfc7074fc62a5342b72d35452d5d0438dfe7eb8d965675bd9917f36fcd. The follow-up runtime(3).log reaches native phase 21 before the ShellUI target becomes unavailable (ESRCH). deck-status.txt reports state 0 and hooks 0. The coordinator reports DECK_UNCERTAIN. There is no working-session input evidence in that log; the menu fix remains unconfirmed on hardware.

## Previous beta.2 observations

ELF: 10,502,240 bytes; SHA-256 7ce148351589545d3db8061acd05ccf7b97f06b9a9203e6abc6e4f73a394bcf5. Firmware 13.60, Payload Manager. The maintainer later reported intermittent freezes/refused launches after both Restart and complete Shutdown/reboot, plus L3 + R3 opening and immediately closing the panel. The attached runtime log identifies source dc4d6b03f3c468089ed2d9717faf090e4c8f9c59812db3cb7db390279c1de3e2 and an unresolved earlier deck.start operation. That log does not capture the initial freeze or the menu flicker.

## Earlier startup recovery baseline

ELF: 10,502,240 bytes; SHA-256 c5a5d508336b2ac2b95cb07e58b2b497af62ce7ae0897db96625c1958bb79b62. The maintainer initially confirmed this startup recovery build worked through Payload Manager. The later beta.2 issues above mean this earlier report must not be presented as proof of general startup reliability.

## Earlier beta observations

ELF SHA-256 8c1403242f9c2ecb0eafe873fbee77c063358a8cc5636a40622d1651c2b5bcc1. The maintainer reported working core timer/storage controls and restored in-game opening after a restart. Capture buttons were unresponsive with the panel open and built-in gameplay recordings omitted the overlay. These remain documented limitations.

The earlier standalone v1.0 test used SHA-256 630697e32ff08f4f3967c38a65dd566a7fa1b79571e20d60cd03dffaea84ea05. All historical reports belong to their exact hashes. None verifies this new replacement.
