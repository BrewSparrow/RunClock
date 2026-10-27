# Standalone console test

The maintainer tested this exact beta ELF on firmware 13.60 through Payload Manager and confirmed in-game opening after a restart. Capture remains incomplete; KNOWN_ISSUES.md and CONSOLE_TEST.md record the current limits. The checklist below is for future testing; it is not a claim that every item passed on hardware.

1. Reboot. Start your usual jailbreak/kernel services, then RunClock.elf alone, with ProsperoMenu disabled for this boot. Wait for the ready notification and open with L3 + R3.
2. Verify the app appears without an installed/running menu. Start run: both visible clocks should tick automatically.
3. Split, pause, wait: active freezes while real elapsed advances. Resume and undo. Test controls while clock digits are changing.
4. Finish after a few splits, confirm both final values freeze and unused slots stay --. Reset clocks: both zero while the completed result stays saved.
5. Start again, confirm-reset an unfinished run, and verify both clocks clear and a reset result is saved. Hiding the panel should cancel pending confirmations.
6. Record all ten splits and finish with the separate Finish run button. Check actual split timestamps and separate final total.
7. On the home screen, open Records and a saved attempt. Triangle should open its deletion prompt; a second separate Triangle press should delete it. Holding Triangle must not confirm automatically. Circle cancels. During a game use the Delete attempt / Confirm delete menu entries: the raw game-input route does not currently forward Triangle. Also test PB recalculation, stable attempt IDs and Circle navigation. Circle on the main timer should close the panel; reopening should refresh the running view.
8. Test on the home screen and with a running game. Confirm gameplay input is released immediately after closing the panel and timing continues in the background.
9. Reboot, reload once, and verify saved records and deletions persist. Active run recovery is not supported.
10. For future capture work, test Create press/hold/double-press with the panel closed and open, both on the home screen and in a game. Inspect the saved screenshot and clip for the overlay itself. Starting a recording successfully is separate from recording the overlay pixels. This beta has no capture fix.

If startup fails, retain /data/RunClock/runtime/runtime.log and runtime/overlays/runclock/overlay.log. Do not load repeated copies in the same boot; the native UI hooks are resident. Reboot before another build. Report the firmware and exact RunClock ELF hash with the result.
