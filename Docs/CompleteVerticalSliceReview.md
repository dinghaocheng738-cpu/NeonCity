# Normal South Beach review entry

Run `Tools/VerticalSlice/OpenCompleteVerticalSliceReview.ps1` after closing the
editor. It opens the existing South Beach map in normal game mode, at a requested
1600x900 window. Audio stays enabled. No automation fixture, forced weapon/loadout,
teleport route, debug map or save reset is enabled. Existing normal persistence
behavior and the player's normal save slot remain in effect. It does not build
the project; use the latest verified binaries for the checkpoint being reviewed.

**Phase 20.5c is still IN PROGRESS / PLAYER EXPERIENCE NOT COMPLETE / ART PASS
NOT COMPLETE.** This is an entry tool for reviewing implemented behavior, not
a claim that all requested activities or the final acceptance route exist.

The current useful checks are:

1. Move through South Beach, interact and view character weapon/health reactions.
2. Enter an available car, view the seated driver, drive and leave safely.
3. Reach the dock, enter the speedboat and use its displayed controls. Check
   acceleration, steering, coasting, reverse, safe exit, swimming and re-entry.
4. Reach the helipad, enter and check the visible pilot and flight instruments.
   Once the current aircraft checkpoint is verified, follow its displayed power,
   flight and existing mounted-weapon controls; land before attempting to leave.
5. Return home and use the existing save/load flow. Keep observations tied to
   the reviewed checkpoint and save rather than replacing normal player data
   with automated test fixtures.

Missing navigation/onboarding, remaining art/audio, the complete marine/aviation
activities and the final 30–60 minute connected route are tracked in
CompleteVerticalSlice.md and VerticalSliceKnownIssues.md. The final review
sequence will be expanded as those real player loops become available; this
document does not substitute for their required in-game controls and markers.
