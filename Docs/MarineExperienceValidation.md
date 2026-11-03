# Marine controls and feedback (Phase 20.5c, incremental)

Status: bounded control/feedback checkpoint verified; **MARINE EXPERIENCE NOT COMPLETE**.
This checkpoint covers the existing speedboat's control response and clear
driving feedback. It does not introduce a yacht or claim final marine art/audio.

| Gate | Current evidence |
|---|---|
| Technical | Editor/Game builds, continuous input test, 12 focused cases, fresh-process persistence and all 117 registered cases pass |
| Player Experience | Scripted drive/exit/swim/reboard route passes; final normal whole-game acceptance remains open |
| Visual | Basic wake, compact spray and readable HUD inspected in actual PIE; hero boat/ocean/yacht art incomplete |
| Performance | Fixed 72-slot effect budget and measured short route; no long stress/whole-city guarantee |

## Physics and ownership

The existing four-point Chaos buoyancy, power-limited thrust, water-relative drag
and velocity-dependent rudder remain authoritative. Input requests signed thrust;
a data-driven rise/fall response produces actual thrust. Reversing first unloads
the engine through neutral. Releasing W/S retains momentum and water drag slows
the hull. Space increases the existing assist drag; no velocity or transform is
forced. Roll/pitch and collisions remain simulated.

FMarineDriveTelemetry exposes 0..1 throttle magnitude separately from direction,
measured water-relative speed, engine/water state and drive state. HUD reads that
same state at 10 Hz using the existing vehicle widget. It is owned only by the
local possessed boat and removed on exit/end play. An unsafe F exit retains
control and tells the player to slow down or clear/level the deck.

Throttle and HUD are transient. Save/load continues to use the existing stable
marine snapshot and restarts an empty boat with its engine off and zero thrust.
No schema or stable vehicle ID changes.

MarineFeedbackComponent owns one spatial audio component, using the reviewed
CC0 motor-loop derivative. Throttle and speed control bounded pitch/volume;
engine stop fades playback out. MarineWaterEffectsComponent owns one collisionless
instanced-quad component, with 48 wake and 24 spray slots. Old foam stays in world
space, fades, and is reused rather than spawning Actors. The effect uses one
registered-water query near the boat, skips beyond 100 m from the local camera
and clears on discontinuous position changes. No effect can push or damage a hull.

## Verification plan and evidence

New ThrottleLoopPIE extends the existing dock route without replacing historic
tests: walk to dock, F enter, gradual W response, sustained thrust, reject unsafe
F with recovery feedback, steer, coast, assist brake, reverse, stop, F exit,
jump into native swimming, swim back and F reboard with clean idle input.
It uses a disclosed initial fixture position; it is not the final continuous
normal-game 30–60 minute acceptance session.

MarineFeedbackEditorBuild01/02/03/05 and MarineFeedbackGameBuild01 pass. Build04
exposed a missing editor-only SlateCore link for the capture window resize; the
explicit dependency fixes it in Build05 without Engine changes. Build06 also
passes the new water-effect implementation. Final EditorBuild08 and GameBuild03
pass with the revised overlapping wake and compact spray, without compile/link
errors. Build07/GameBuild02 are successful intermediate iterations.

MarineThrottleLoop01 passes the continuous physical/input loop with actual audio
playback start/fade checks. It captures 1600x900 3D frames but does not capture
UMG. Loop02 adds actual Slate screenshots; Loop03 resizes the test window and
passes again, with inspected cruise/exit/reverse HUD images. Slate currently
captures its client area at 1264x711 while the separate world readback remains
1600x900; do not call that a final 1600x900 composited performance capture.
Loop04 passes physics/audio/particle-lifecycle checks, but inspected frame 181
shows round, separated foam stamps and smoky spray. That visual defect was not
accepted. Loop05 shortens spray, lowers opacity and stretches overlapping wake
stamps along the travel direction. Its real frame 161 and exit-feedback HUD were
inspected: the continuous trail follows the hull and feedback is readable. The
whole drive/turn/coast/reverse/exit/swim/reboard test passes again. Effects retain
the same 72-slot cap and no physical forces. This is basic water feedback, not a
final ocean/wave simulation or a finished speedboat visual pass.

MarineFeedbackFocused01 passes all 12 selected unique marine, swimmer, occupant,
character, mounted-weapon and persistence cases. MarineFeedbackCrossSession01
passes the existing marine A/B test in separate OS processes (10500 and 30828):
save ID, stable vehicle ID, 50% condition, ownership and transform match. Session B
loads through the real SaveSubsystem, restores an empty engine-off boat and
cleans its dedicated test slot. The new registered test brings the full queue to
117; MarineFeedbackFullRegression01 passes 117/117 unique names, zero case
errors, no omissions or duplicates. Existing evidence and the
rejected Loop04 appearance are retained.

The no-readback short boat route in that full queue records 372 samples at the
declared 1600x900 / Epic / 100% baseline. Median/p95 timings in milliseconds are:
frame 16.664/17.758, GPU 7.047/8.491, game thread 7.551/8.201, render thread
5.817/6.389. Raw CSV and MarineFeedbackShortRouteMetrics.json remain under Saved.
These are instrumented short-route measurements, not a before/after comparison,
whole-city performance guarantee or the required long-session stress acceptance.

Coverage accounting: 117 unique registered names are expected in the full queue,
including the one new ThrottleLoopPIE case. The 12 focused executions repeat a
subset, and Loop01–05 repeat the same new case; they are not added to unique
coverage. A/B are reported separately as a two-process disk-persistence scenario.
Optional image-review helpers in the full metrics queue do not provide visual
acceptance; inspected captures are identified separately above.

## Open marine gates

- Final speedboat/yacht exterior/interior artwork and complete yacht loops.
- Basic bounded wake/spray is imported, visible and checked in actual PIE;
  physically richer water, final ocean artwork and broad visual acceptance remain
  open. The current wake uses flat registered-water height and has no fluid solver.
- The author's public CC0 motor preview has a separate mono-loop derivative and
  telemetry-driven spatial playback component. Official import and audio-enabled
  runtime playback checks pass; audible quality acceptance remains pending.
- Final performance and coherent whole-game UI/audio review.

Source discovery precedes any audio intake. Current author-page candidates and
download/access decisions are in ResourceDiscovery.md. The acquired public motor
preview has not been claimed as heard or given final sound-quality approval.
