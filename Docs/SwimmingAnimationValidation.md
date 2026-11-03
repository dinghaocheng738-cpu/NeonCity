# Swimming animation diagnosis and acceptance

The pre-change normal South Beach player was BP_ThirdPersonCharacter, using
SKM_Quinn_Simple / SK_Mannequin and ABP_Unarmed. Rendered PIE reported native
MOVE_Swimming (4), but the active final land assets were MM_Idle when idle and
BS_Idle_Walk_Run with weight 1 while moving. Therefore this was **native swimming
with missing swimming animation integration**, not just a boolean/state problem.
The actual water volume and native movement were already functioning.

## Assets and graph

Five original, editable Blender actions are baked on the project-owned Epic rig:
Tread, Forward, Underwater, Ascend, Descend. Each is 2.4 seconds, 30 fps, 73 samples
including the matching endpoint, with stationary root/no root motion.
Tread uses sculling, alternating eggbeater legs and torso motion; the locomotion
cycles use coordinated pull/recovery, delayed frog kick, glide and head extension.
These are authored prototype cycles, not purchased motion capture or a claim of
finished animation quality. Hand trajectories, kick timing and shoulder deformation
are explicit review points for an animator/user.

`USwimmingAnimInstance` supplies presentation weights and play rate from actual
CMC velocity, mode, body depth and CanAct. `ABP_NC_Swimming` links the unchanged
Epic ABP_Unarmed as its land branch and blends the five real AnimationSequences
as a complete final water branch. At WaterAlpha=1 the land foot IK/land layers
cannot overwrite the swimming output. No template movement code is copied or
changed. Existing combat restrictions still disable weapons in swimming.

The diagnostic asset dump includes cached values from linked graphs. An inactive
land graph can retain its last node weight even while the final WaterAlpha blend
fully excludes it. Final-output verification therefore also asserts WaterAlpha,
samples the rendered hand sockets across a cycle and records continuous viewport
frames; a cached land node name alone is not evidence that walking reaches the output.

Pose selection uses capsule/body depth and explicit dive intent rather than the animated head: a diving
pose must not lower the head and keep its own transition condition true indefinitely.
Oxygen separately samples the real `head` socket plus 8 cm, filtered at 20 Hz,
with enter/exit depth hysteresis of +6/-5 cm. Teleports reset that filter.
The shallow surface band remains under native movement input control, with a
target capsule depth of 0.65×half-height. Pressing C latches dive intent until
ascending back into the surface band or leaving swimming (including load/boarding).
This prevents the surface assist from undoing a deliberate shallow dive as soon as
the key is released. Deep diving retains neutral buoyancy.
There are no per-frame actor transforms, capsule rotation or root-motion displacement.

CanAct=false removes swimming animation priority so existing injury/death handling
retains authority. Load/respawn use the blueprint's saved wrapper reference; boat
boarding uses the same original character. A dedicated cinematic death swim pose,
water-entry dive and climb-out animation are outside this asset set; transitions
currently blend native states and existing land motion.

## Automated acceptance

`NeonCity.Visual.SwimmingAnimationAssets` checks actual skeleton, durations,
sampled limb/pelvis motion, matching loop endpoints, stationary root and graph parent.
`NeonCity.Visual.SwimmingRoutePIE` walks down actual shore steps, checks visible
tread and moving hand sockets, swims/turns/dives/ascends, verifies oxygen and weapon
restrictions, walks up shore and jumps on land. It places the player only before
the route begins; no teleport is used to complete the sequence.

The existing Marine.DeckSwimmingPIE records native moving deck, jump, swim back
and F reboarding when -NeonCityArtMotion is set. Existing water tests retain
drowning, incapacitation, submerged car escape and safe load coverage.

First full A route passed in `Saved/VisualOverhaul/RouteFirst` and again in
`MotionTrial-20260923`; these are development trials, not final acceptance evidence.
Final continuous sequence and regression results are recorded separately below.

Human acceptance remains pending: inspect multiple full cycles, silhouette from
normal chase camera, limbs under clear water, blend transitions, mouth/waterline,
shore exit and boat sequence. A single image cannot establish a good animation loop.

## Final continuous capture — 2026-09-23

`Saved/VisualOverhaul/ReviewMotion-20260923` contains the final current run.
The five Visual tests and separate Marine.DeckSwimmingPIE all reported Success,
zero assertion errors. ReferenceReviewPIE is an opt-in diagnostic and is separately
executed with its capture flag; its default entry is informational.

| Sequence | Frames | Recorded game duration | Recorded wall duration |
|---|---:|---:|---:|
| A: shore / tread / swim / turn / dive / ascend / shore / jump | 493 | 44.084 s | 44.084 s |
| B: native moving deck / jump / swim return / F reboard | 127 | 11.245 s | 11.245 s |
| C: frontage / real entrance / walk to sedan / F / drive / exit / night | 477 | 43.048 s | 43.046 s |

All frames are actual 1600×900 viewport readbacks, JPEG quality 92, sampled at up
to 12 Hz. The local `Review.html` replays their recorded game timestamps; original
`frames.csv` files retain frame, game time, wall time, phase, pawn and movement
mode. No artificial interpolation or image beautification is applied. This is a
continuous frame sequence, not a claimed 60-fps screen recording.

The same final frames are also available as A-Swimming.mp4, B-Boat.mp4 and
C-Street.mp4. Blender's sequencer repeats those frames at the CSV game timestamps
in a 30-fps H.264 container; no frames are synthesized. All three files were
reopened and verified at 1600x900 (1326, 340 and 1294 container frames respectively).

The first PNG-recording trial stalled physics through synchronous compression and
was stopped as an invalid capture. JPEG reduced the overhead. The same route then
passed; performance numbers come from a separate capture-free measurement interval.
The earlier route assertion was also corrected to check full forward pose during
cruise, before intentionally slowing for the turn, rather than incorrectly demanding
full-speed animation at a stop. No gameplay threshold was relaxed to make that pass.

The first integrated full regression (Aircraft-20260923-123954) passed 102/103.
Water.SwimmingPIE exposed a real shallow-dive regression: the animated mouth entered
water before the old upright capsule top, the test released C, and surface assistance
immediately returned the player to air. The runtime now retains explicit dive intent;
the original test was not weakened or changed. All six water checks and SearchLightPIE
then passed in WaterLighting-20260923-125944. The final full-suite rerun is recorded
in VisualOverhaul.md.

These tests establish functional continuity and real animation playback, not an
animation director's approval. Entry/exit blends and authored swimming technique
still need user review, particularly the frog-kick silhouette and shoulder range.
