# Physical foliage interaction — 2026-09-26

The user requires tangible trees and vegetation, in addition to a denser mountain
silhouette. This checkpoint adds contact behavior to the existing reviewed
Tidewater vegetation. It does not modify the Third Person Template's movement,
Chaos vehicle settings, combat damage rules, or the original source scatter.

## Hard wood

`PrepareTidewaterWoodCollision.py` extracts only material-part-zero bark from the
reviewed source geometry. It verifies that all three original tree variants have
identical bark. The derivative contains 526 vertices and 638 triangles: trunk,
buttress roots and branches, without leaves or a solid crown box. Editable Blender,
FBX, source hashes and native package receipts are retained. The existing MIT
notice applies; no external download or new tree model was introduced.

The native mesh uses double-sided complex-as-simple collision on non-simulated
components. A nearby component pool enables QueryAndPhysics and blocks Pawn,
Vehicle, Camera, Visibility and the existing weapon trace channel. Friction is
0.7 and restitution 0.02. Wood has an explicit shared physical surface ID. These are
stationary rooted trees, not light rigid bodies. Chaos supplies the vehicle's
contact response once; its existing PhysicalImpactComponent then emits the
existing damage and feedback events. No custom impulse is added to the player
or vehicle, and no actor transform is attached to a tree each frame.

## Soft contact and bounded work

At startup each vegetation layer builds a 16 m spatial-cell index. Runtime
queries inspect cells around the local player and up to seven nearby pawns.
The pawn list refreshes once per second, or immediately on possession changes.
Wood activation refreshes at 10 Hz and includes a half-second velocity corridor
plus a safety margin. It uses distance hysteresis and reuses a maximum of 512
wood bodies per layer (the palm follow-up reserves 160 for curved stems).
Allocated bodies are not additional tree actors.
Inactive bodies disable collision; unused visual instances remain normal HISM
rendering with the existing source wind and near/far representation.

Shrubs, ferns and the other soft understory use at most 192 active vector springs
per layer. Actual nearby pawn volumes set their bending targets. A 20 Hz update
with small integration substeps applies damping and returns them to rest after
contact. Displacement is bounded at 75 cm; the source wind is retained. The first
eight instance-data floats remain the exact original shader contract. Three
additional floats carry native displacement. The shader anchors the roots and
excludes hard tree bark. UE's incremental instance updates send only changed
instances; a complete forest render-state rebuild is not performed per update.

## Verification

- `TidewaterFoliageContacts01`: 2/2 focused tests pass. The data check verifies the
  cooked bark-only collision policy. Actual PIE verifies character blocking,
  empty space beside the trunk, no ordinary-contact injury, soft deformation in
  the rendered instance, spring recovery, unchanged roots, and a real Chaos crash.
- Recorded contact: soft bend peak **28.14 cm**; crash normal speed **1722.02 cm/s**
  (about 62 km/h); vehicle health **100.00 -> 49.63**, through shared damage.
  The vehicle is stopped/deflected and cannot pass through the trunk.
- `TidewaterPhysicalJungle01`: the saved dense-island hiking route passes with
  physical trees enabled. Peak active wood **53**, queried candidates **654**,
  capacity misses **0**. The original `TidewaterPhysicalForest01` 80 m route also
  passes without test-only tree hiding, collision disabling, or route relaxation.
- `TidewaterFoliageBuild04` Development Editor Win64 and `TidewaterFoliageGame04`
  Development Game Win64 succeed, zero compile/link errors. The initial Build01
  caught three TObjectPtr deduction errors; those were fixed before successful
  builds and PIE. Existing toolchain/API warnings remain visible.
- Source/package validation passes: **142** generated art files, **184** reviewed
  external files, **591** managed packages, **354** Tidewater derivative files.
- `TidewaterFoliageContactVisual01` passes with normal-player viewport captures
  and continuous frames. Inspected character/trunk and soft-contact frames show
  the source geometry and retained player rig. This is deliberately a controlled
  fixture on an invisible collision floor, not a finished forest beauty shot.
- `TidewaterPhysicalJungleMetrics01` passes without continuous screenshot
  readback. After four seconds of warm-up, 312 samples at 1600 x 900 on RTX 5060
  Laptop report GPU median/p95 **35.740 / 49.306 ms**, wall frame **37.254 / 50.578
  ms**, game-thread **7.273 / 7.944 ms**. The same pre-contact JungleMetrics03 route
  reports GPU **35.502 / 48.850 ms**, wall **36.909 / 50.136 ms**, game-thread
  **6.696 / 6.960 ms**. These single runs indicate a small extra CPU cost; they
  do not prove a statistical performance improvement or a stable 60 FPS target.
  Dense forest is currently GPU limited and needs further rendering work.
- Full `TidewaterPhysicalForestRegression01`: **144/145 Success**, 1842.03 s.
  The sole failure is FixedWing.ApproachCityPIE's fixed-time city crossing
  assertion; all physical foliage, forest routes, combat, wanted, vehicles and
  persistence cases pass. The unchanged aircraft/test then passes in a fresh
  `TidewaterApproachRecheck01` process. The failed run includes a render/asset
  stall before its fixed 2.5 s sample; the old test did not log that endpoint.
  This supports a timing problem, but does not retroactively make the full run
  green. The test now logs its endpoint and waits at most 6 s for exactly the
  same X > 2000 / Z > 1700 crossing, retaining the 2.5 s minimum observation.
  No aircraft controls, physics or health assertions were changed. A complete
  green run remains required after the next native integration checkpoint.
- Final `TidewaterFoliageBuild05` Editor (20.17 s) and `TidewaterFoliageGame05`
  Game (18.27 s) builds succeed. `TidewaterFoliageCloseout01` passes all three
  focused aircraft/wood/contact cases. Logged city crossing is X=4614.590,
  Z=1959.965 cm at 2.500 s, with aircraft health 700. Original full-run evidence
  and its one failure remain retained; focused reruns are listed separately.

The first placement attempt rejected the old eight-float schema check before
saving the map. Both placement tools now require eleven floats for near groups
and eight for distant cards; the native saved map was then rebuilt successfully.

## Deliberate limits / remaining work

- The first checkpoint omitted mature coconut palms. The follow-up adapter uses
  ten capsule segments following the source rest stem curve and radius profile.
  It preserves the original per-instance height, lean, azimuth and seed; height
  is not accidentally multiplied by radial scale. The shared 512-body allocation
  now reserves up to 160 bodies for sixteen nearby mature palms and 352 for
  broadleaf wood. Curved empty space and the leaf crown remain nonblocking.
  Tiny asymmetric root lumps and high-wind stem flex are visual approximations.
  Follow-up native contact tests/builds are recorded below when complete.
- Hard branch collision follows the rooted source rest geometry. It does not
  follow every wind-driven twig vertex. Trees do not break, fall or uproot.
- Soft bending is a local spring presentation response, not soft-body leaf
  collision or force feedback hardware. Contact-specific rustle output is now implemented and tested; see
  FoliageContactAudioValidation.md.
- Only nearby wood is enabled. Remote projectiles and offscreen simulation outside
  the local interaction region are not certified as being occluded by distant
  trunks. Multiplayer, split-screen and island-wide concurrent simulation need
  explicit observer/budget policies. Capacity misses are exposed in diagnostics.
- The enlarged cache clears the prior Lumen oversubscription warning on the
  inspected trail, but does not certify every scene. VSM marking overflow remains
  on some workloads, and sunlit gaps are still excessively bright. The foreground
  mask defect is now identified and corrected; see ForestRenderingValidation.md.
  Physical test success does not certify those visual/performance
  gates, the complete marine/island route, or completion of Phase20.5c.

## Curved-palm follow-up

TidewaterPalmContactBuild01 succeeds for Editor (21.61 s) and Game (32.09 s),
including the new grass integration. Its first contact fixture incorrectly
expected a palm outside the activation radius to already have collision. The
fixture now checks inactive-at-distance, followed by activation on real approach.
Visual inspection also found a real issue: capsule joints could serve as steps.
Palm wood now explicitly rejects step-up and walkable-floor classification,
without modifying CharacterMovement or disabling solid collision.

TidewaterPalmContactBuild02 succeeds for Development Editor Win64 (9.75 s) and
Development Game Win64 (14.22 s), no compile/link errors. TidewaterPalmContact02
passes the expanded real-PIE case: original character/wood and shrub recovery,
real Chaos impact, curved-stem contact at three heights, empty-side clearance,
normal player blocking and no capsule-joint climbing. The final normal viewport
was inspected. This is the same controlled fixture on an invisible floor, not
an authored-island beauty image. Reported allocation is eleven bodies, soft
bend peak 26.20 cm, vehicle normal impact 1737.09 cm/s, health 100 -> 48.60.
The engine motion-vector console warning remains recorded.

File validation passes 142 generated files, 184 reviewed external files, 597
managed packages and 361 Tidewater derivative files. Native saved-map routes,
night comparisons and complete 147-case regression follow this integration.

TidewaterForestFinalMetrics01 subsequently passes the native 133 m route with
all current palm/grass/rendering changes: 53 active wood plants, 695 queried
candidates and zero capacity misses. Median wall/GPU/game-thread costs are
20.797/19.557/6.505 ms (354 samples after four seconds of warmup, 1600 x 900,
no continuous pixel readback). One existing motion-vector warning remains.
The full 147-case integration run is still pending; timing improvement is not
a claim of stable 60 FPS, complete visual acceptance or an island-wide physics
simulation.

## Complete integration result

TidewaterGrassFoliageRegression01 passes **147/147** registered cases in 1731.05 s
(145 plain successes, two successes with existing engine warnings). No missing,
extra or duplicated cases; no test errors or reflected-class startup errors.
PhysicalContactPIE, both forest routes, grass, vehicles, combat, wanted and
persistence all pass in the same process. The two warnings are motion-vector
console access (LivingCityPIE) and VSM marking (GroundFloraWalkPIE). Previous
failed runs remain documented; this new full run closes the integration regression
gate for the current grass/palm/physical-forest checkpoint only.

## Physical feedback and attributes follow-up

TidewaterFoliageFeedbackRegression01 now passes 151/151 in 1755.99 s, zero errors,
with only the known motion-vector warning. Native contact audio, root-anchored
spring recovery and physical tree/curved-palm blocking remain green together with
all historic vehicle, combat, wanted and persistence tests. Final Editor/Game
builds pass with zero compile/link errors. See FoliageContactAudioValidation.md
for the separate audio-enabled recording and 236-frame physical fixture replay.
Hard wood remains rooted/kinematic; this does not add tree fracture, falling,
uprooting, whole-island collision or haptic-device feedback.


## Wood surface and real weapon occlusion — 2026-09-27

The rooted broadleaf/palm bodies now identify `SurfaceType4` as Wood. Existing
Phase 15 IDs 1/2/3 remain Concrete/Metal/Glass, with the same friction, restitution
and damage scaling. `ENeonCityImpactSurface::Wood` is appended without changing
older enum indices. Hitscan queries request the actual hit physical material;
the common `CombatSurface` resolver also handles a body material fallback and
retains legacy character/vehicle/prop classification. It does not depend on a
player or foliage class. No new fracture, splinter effect or wood impact sound
is implied by this event classification; contact rustle is the existing separate
soft-plant output.

`FoliageWoodSurfaceChecks01` passes 2/2: existing Combat.FoundationPIE and the
expanded Foliage.PhysicalContactPIE. The latter fires the actual unmodified pistol
from a generic mount into a broadleaf trunk and curved palm. Both shots consume
a round, terminate on wood and leave the target behind it at 100 health. A
matching clear lane beside the trunk reaches the target and applies 25 damage.
This complements normal character blocking, spring recovery and real Chaos
impact (1722.02 cm/s, car health 100 -> 49.63), which now reports Wood through the
same physical impact event. Peak spring displacement is 27.05 cm, eleven bodies,
no capacity misses. One existing motion-vector warning belongs to Combat's
fixture; no test errors. Editor compilation succeeds in 44.51 s.

This proves nearby active collision and surface classification. It does not
extend distant projectile collision activation, turn leaf crowns solid, add
wood destruction, or certify visual/human feel acceptance. The complete broader
Phase20.5c work remains in progress.


FoliageWoodSurfaceGame01 Development Game Win64 also succeeds in 40.79 s,
zero compile/link errors. Full FoliageWoodSurfaceRegression01 passes **151/151**
in 1754.51 s, zero failed/not-run/missing/extra/duplicated cases or test errors.
The sole warning is the existing motion-vector access warning in LivingCityPIE.
This integrated run includes the retained cache-material correction as well as
wood material/actual-shot checks and all historic systems. Peak contact bend is
28.14 cm; eleven bodies; real car impact 1722.02 cm/s, health 100 -> 49.63.

Frozen Editor/Game hashes and the staged implementation tree match
Saved/VisualOverhaul/FoliageWoodSurfaceRegression01/TestedSnapshot.json. The
parallel preparation of unlinked ocean shader/audit files was not loaded into
this game build and is not certified by these 151 tests. Daylight balance,
whole-world performance, subjective feel and broader Phase20.5c remain open.
