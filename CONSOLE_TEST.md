# Console verification history

## Tested beta executable

- Version: **RunClock v1.0.0-beta.1**
- Developer shown in the app: **BrewSparrow**
- Firmware: **13.60**
- Loader: **Payload Manager**, with existing compatible kernel support
- ELF size: **10,502,240 bytes**
- ELF SHA-256: `8c1403242f9c2ecb0eafe873fbee77c063358a8cc5636a40622d1651c2b5bcc1`
- Report date: **2026-10-27**

The maintainer initially reported that core functionality worked as expected on the physical PS5. Later testing found that L3 + R3 opened the panel on the home screen but not during games. A subsequent console restart restored opening RunClock while a game was running. This is a reported recovery on the tested setup, not a new code fix.

The maintainer also reported that capture buttons did not respond with RunClock open, and subsequently that RunClock was absent from a recorded gameplay clip. **Capture support remains incomplete.** The release is a public beta with these limitations documented in KNOWN_ISSUES.md.

Source inspection additionally identifies an unmapped Triangle shortcut in the raw game-input route. The Delete attempt / Confirm delete menu controls remain the in-game deletion path. The complete game-input relay, screenshot path, recording composition, and hardware timing latency are not covered by the automated host tests.

The beta ELF is unchanged during release finalization. Documentation, checksums, archive packaging, and the release-asset verifier are updated; no renderer, game-input, or capture code is rebuilt for this release.

## Earlier baseline

The maintainer reported restarting the PS5 and successfully testing standalone v1.0 on 2026-10-27. That test belongs to the earlier 10,502,240-byte ELF, SHA-256 `630697e32ff08f4f3967c38a65dd566a7fa1b79571e20d60cd03dffaea84ea05`, on firmware 13.60 through Payload Manager. It is retained as history and does not replace verification of the beta ELF above.

See PS5_TEST.md for the next-build checklist and VALIDATION.md for host-test scope.
