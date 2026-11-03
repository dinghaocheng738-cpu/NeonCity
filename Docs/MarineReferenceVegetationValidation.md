# Canonical vegetation — direct conversion work

Pinned source: dgreenheck/tidewater at
`4811ba48d795197de5621985f404e765c0b7c0ef`, MIT, DRG Software Solutions LLC (2026).
The full notice remains in `ThirdParty/Tidewater-LICENSE.txt`.
**VEGETATION PARITY FAIL until native forest and continuous player routes pass.**

The actual source is procedural MIT geometry, not downloaded unknown tree models.
`ExportTidewaterVegetation.mjs` runs the canonical terrain seed 7, village seed
90210, site seed 1234 and scatter seed 99. It uses the actual village footprint
and sampled boardwalk exclusions, exact terrain detail field and source rules.
Headless adapters suppress only GPU upload/bake execution and record the actual
generated atlas program; the pinned checkout remains unchanged.

The immutable, hashed intake contains 12,745 trees, 5,598 shrubs, 233 adult palms,
188 banana clumps, 268 young palms, 44,548 ferns, 1,693 monsteras, 24 elephant ears,
70 heliconias and 16 bird-of-paradise plants. These are source counts, not a promise
to render all geometry at full detail. It also preserves the 1024-square RGBA
dune/meadow/sea-oat/beach-creeper mask, eleven mesh buffers, three exact tree crown
variants, two shrub variants, leaf atlas program and full source surface/wind code.

Native integration must retain source per-instance height/lean and mesh attributes:
raw palm crowns are relative to the stem tip and cannot be imported as finished
plants without their vertex deformation. Near vegetation uses bounded instancing;
far trees must retain their source crown shapes and forest silhouette. The source
uses hemi-octahedral impostors, not disappearance beyond near-detail distance.
Grass, collision, materials, wind, distant canopy and normal-game before/after
validation are still open. No Blender or asset review is game acceptance.

## Initial native world evidence — 2026-09-26 (before far handoff)

The native library and typed scatter are now placed in the existing island map:
eleven meshes, two linear data textures, two source-equation materials and one
Data Asset. Thirteen HISM groups retain all 65,383 instances. Two shared runtime
materials carry wind; there is no per-tree Tick. Rebuilding validates transforms,
source-coordinate correspondence and upright axes before replacing components.

`TidewaterForestBefore01` records ordinary player-camera views at the pier head,
hill and lower forest. `TidewaterForestGeometryAfter01` exposed a real conversion
failure: Python's NativeMakeFunc takes Roll/Pitch/Yaw, unlike the C++ FRotator
constructor. Positional yaw became pitch, tipping trees sideways. Import now uses
named fields and checks every instance. `TidewaterForestUpright02` passes and all
three matching views show upright source plants and hillside canopy. Failed images
are retained. `ReferenceVegetationData` guards counts, all axes, scale and shader
base correspondence in the native Data Asset.

These are matched normal-game fixtures with initial placement at each view, not
the required continuous boat/landing/climb/helicopter route. Current tree/shrub
geometry still needs the canonical 180-view impostor handoff; no far-performance
claim is made. Full source grass, trunk collision policy, island streaming/ocean,
combined frame-time measurement and continuous player acceptance remain open.

The user's subsequent colour correction is implemented as an explicit native
`HillCanopyVibrancy` material parameter. Across 15–80 m source elevation it
smoothly increases non-bark, non-shrub leaf brightness and green saturation,
retaining the original species variations, leaf age, canopy AO and lighting.
It changes albedo/transmission, never emissive light. The canopy's authored .15
specular value is now connected instead of UE's .5 default, avoiding grey glare.
Source records and the pinned repository are unchanged. The far impostor below
applies this same grading, so LOD cannot restore the old muted colour.

`TidewaterForestColour03` records the initial grade; `TidewaterForestColour04`
is the final same-camera review with corrected specular. All three ordinary
game views were inspected: the hillside reads greener, foreground species retain
their distinctions and shaded canopy remains dark. This confirms the requested
colour adjustment, not the still-pending continuous route or vegetation parity.

`TidewaterForestLightingRegression01` completes all 139 registered tests. A forest
VSM marking-queue warning means the current all-geometry far rendering is not a
performance pass. Two startup colour-field initialization errors were discovered
outside the test results and fixed explicitly; the validation runners now reject
these pre-queue errors. Editor and Game rebuild successfully in
`TidewaterForestDefaultsBuild01`; all four focused checks in
`TidewaterForestDefaultsChecks01` pass without reflected-class initialization errors.

## Canonical far canopy — 2026-09-26

The native map now uses five source tree/shrub variants and all 180 canonical
hemi-octahedral views. BakeTidewaterImpostors captures source brightness, leaf/bark
fraction, radial coordinate, normal and crown AO as data. PrepareTidewaterImpostorTextures
validates every raw pass and produces two linear 3840 x 768 RGBA8 atlases. These
SceneCapture passes are conversion data, never game acceptance evidence. Source
frame bases require a horizontal flip and the source bottom-to-top row mapping.

Three failed calibration attempts are retained. The final Bake04 uses an explicit
separate forward vector; negating the camera direction through the Python binding
had left the camera looking away from its subject. All 540 raw passes and a linear
colour calibration pass are validated, including finite values, per-pass matching
coverage, normal lengths and nonempty view cells. No screenshot was painted into
the atlas. PortTidewaterImpostors converts the original vertex/surface and temporal
handoff programs to native HLSL; Shader04 standalone DXC compilation passes.

The native library now has twelve meshes, four textures, three materials and one
Data Asset. Thirteen near groups retain 65,383 unique source plants; five extra
HISM groups represent the same 18,343 distant trees/shrubs. Near transitions use
the source 65/45 m distances and 12% band; three frame sampling gives way to the
nearest frame beyond 100 m. Shrubs end at 180 m; trees reach 2,800 m. The actor
updates three shared materials, never individual plant transforms. Main gameplay
view drives LOD independently of shadow views. This is currently single-view.

Native visual differences are explicit: all source crowns are retained by default
(optional source 0.5 thinning remains available), and the minified coverage cutoff
is 0.30 instead of 0.42. The user's hill-green grading is identical in both
representations, with no emissive light. Near canopy bounds are reduced to 2 m
local padding, justified by worst-case source wind/scale bounds at 25 m/s:
tree displacement <= 1.511 m horizontally / 1.028 m vertically; shrub displacement
<= 0.321 m / 1.179 m. The proof remains in Vegetation/Import.json.

TidewaterForestImpostors01/02/03 record the source thinning, full-density, and
coverage adjustments respectively. Bounds01 and ShadowPolicy01 have all three
ordinary player views inspected. Hill/far leaves retain the greener treatment;
forest has real nearby cast shadows. Current pale ground/ferns and camera occlusion
by noncolliding branches are still visual limitations, not accepted full parity.

The initial far conversion incorrectly inherited near cast shadows. The original
Vegetation.js far parts use LodLevel's castShadow=false. Restoring that policy
retains source crown AO and received lighting, while removing the native VSM
marking overflow in ShadowPolicy01 and ShadowMetrics01. Near groups retain their
eleven original shadow casters. No global shadow budget or error suppression was
used. The engine r.MotionVectorSimulation render-thread warning remains.

ForestHandoffWalkPIE follows 80 m of normal template movement after one initial
fixture placement, preserving health and ground support at all waypoints.
ForestWalk01 records the actual player frames; its foreground branch occlusion
is retained as a limitation. ShadowMetrics01, without continuous pixel readback,
passes in 21.00 s. After the first three seconds, 193 samples show
GPU median 13.651 ms / p95 14.805 ms and frame median 16.668 ms / p95 17.829 ms on
RTX 5060 Laptop at 1600 x 900. The before-policy GPU median was 13.670 ms; this is
not a measurable steady-state speedup claim. The benefit verified here is removal
of the overflow, with stable local traversal. Combined ocean/island workload is
still unmeasured. Render-thread timer values are not treated as reliable profiling.

The first atlas data check read a 32-pixel asynchronous editor placeholder;
Data02 waits for asset compilation and then checks exact dimensions, mips and
linear sampling successfully. CanopyBuild04 caught a TObjectPtr deduction error
in the new shadow-policy test. The explicit native pointer fix is followed by
CanopyBuild05: Editor 5.90 s, Game 18.01 s, zero compile/link errors. Engine API
deprecation and non-preferred toolchain warnings remain visible in the logs.

TidewaterCanopyRegression02 completes all 140 registered tests with zero errors,
missing or duplicate tests, and no reflected-class startup errors. Two in-test
warnings remain: the engine motion-vector warning in LivingCityPIE, and a VSM
overflow in the separate CoastalLoopPIE workload. The focused island checks no
longer emit that overflow; this is not a global zero-warning performance pass.

The user has explicitly rejected the remaining sparse distant mountain cover and
requested a jungle exploration experience. Supplemental canopy and layered trail
vegetation remain a new visual acceptance gate; source counts alone are not enough.
This historical checkpoint is followed by `MountainJungleValidation.md`: a
separate layer now adds 20,510 trees and 8,525 other plants. The 133 m source
hiking leg and earlier 80 m handoff walk both pass with actual bark collision.
`FoliageInteractionValidation.md` records wood/Chaos contact and soft-plant
springs. Native camera clearance and Nanite near crowns are also implemented.
The denser hiking workload again exposes VSM overflow and visible Lumen cache
pressure, so the older sparse-route warning/13.651 ms result must not be applied
to this newer workload. Its current visual/performance gate remains open.

Grass, palm far LOD and mature-palm collision, full island streaming, shore/ocean
effects and the continuous boat/landing/climb/helicopter route remain required.
