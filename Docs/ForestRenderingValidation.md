# Dense forest rendering investigation — 2026-09-26

This is an integration checkpoint, not Phase20.5c visual acceptance. Physical
contact evidence is recorded separately in FoliageInteractionValidation.md.

## Confirmed mask defect

The engine-generated SM6 material shows that DitherTemporalAA returns a signed
clip threshold, not a binary 0/1 mask. At complete coverage its output can be
0.5. Multiplying distance fade, palm representation and camera clearance could
therefore produce 0.125 at complete coverage, below the material's 0.5 cutoff.
It also allows two rejected negative thresholds to cancel. This caused regular
screen-door holes on otherwise solid bark as well as leaves.

ImportTidewaterVegetation now intersects thresholds with minimum. Original leaf
silhouettes, the native cutoff, source wind, distance transitions and physical
wood remain. Camera clearance excludes bark. TidewaterForestMaskFix01 completes
the real 133 m trail and its inspected BaseColor diagnostic confirms intact bark
and leaf surfaces. The earlier incorrectly named BaseColor image in
TidewaterForestDiagnostics01 was actually Lit (the console rejected the view-mode
name); it must not be used as a base-colour diagnostic. Diagnostics02 uses the
engine's VisualizeBuffer name and asserts the show flag.

Lumen's close orthographic surface-cache cameras must not apply gameplay camera
clearance or representation fade. A RayTracingQualitySwitch now uses the actual
leaf silhouette for the cache/ray branch. This follows the engine's documented
[view-dependent material guidance](https://dev.epicgames.com/documentation/unreal-engine/lumen-technical-details-in-unreal-engine).
TidewaterForestCacheMask01 compiles, walks and captures successfully; this change
does not by itself resolve the excessively bright sunlit gaps.

## Cost and memory investigation

At Epic settings the default directional VSM resolution bias is -1.5. The dense
moving forest's profiled shadow HW raster pass cost 28.693 ms. The current project
Epic preset uses -0.5 for both stationary and moving directional lights, retaining
all original near shadow casters. It does not disable dynamic shadows, wind or
diagnostics. See the engine's [VSM resolution documentation](https://dev.epicgames.com/documentation/unreal-engine/virtual-shadow-maps-in-unreal-engine).
All other quality presets keep engine defaults.

The Epic Lumen surface-cache atlas grows from 4096 to 5120, because the dense
native canopy previously exceeded its page budget by about 32%. In the inspected
Diagnostics02 capture that red warning is absent; sampled GPU memory use was
5832 MiB on an 8151 MiB RTX 5060 Laptop. This is one route/sample, not certification
of all world-streaming or 8 GB GPU workloads. VSM overflow remains separately
visible in some earlier runs and is never suppressed by configuration.

No continuous screenshot readback is used in the following timing runs. Samples
with game_seconds <= 4 are excluded, and values are medians at 1600 x 900:

| Run | Samples | Wall ms | GPU ms | Game thread ms |
|---|---:|---:|---:|---:|
| TidewaterForestProfileMetrics01 | 352 | 38.905 | 37.676 | 6.955 |
| TidewaterForestWindMetrics01 | 351 | 39.102 | 38.109 | 6.955 |
| TidewaterForestShadowMetrics02 | 354 | 21.073 | 19.849 | 6.496 |
| TidewaterForestFinalMetrics01 | 354 | 20.797 | 19.557 | 6.505 |

The ShadowMetrics02 run includes the mask correction, larger surface-cache budget and VSM
bias change, so it is not an isolated measurement of any single change. Its
profiled shadow HW raster pass is 9.224 ms. Async queue events overlap and must
not be summed into a frame-time claim. A stable 60 FPS gate remains open.

A separate 30–40 m tree-wind fade / 45 m WPO distance experiment showed no useful
improvement (28.746 ms shadow raster); it was reverted in both code and native
materials. Full source wind remains. Capture runs deliberately cost more and
must not be compared to the timing table.

TidewaterForestExposureProbe01 temporarily compared the cached-lighting offset
at 8 and 4 EV in the same stopped view. The latter did not repair daylight balance;
8 was restored. That temporary test toggle was removed, with its evidence kept.
No production exposure adjustment resulted from this experiment.

## Current visual limits

At the diagnostic waypoint one clock reports 11:00, 72444.438 lux sun and a
0.974 real-time skylight. Despite this, forest shadow metering is very dark and
sunlit gaps clip to white. The exposure/indirect-lighting cause is under active
investigation. Increasing brightness or hiding warnings is not acceptance.
Native grass looks too pale under that lighting. PalmContactBuild02 now passes
both Editor and Game. GrassPalmWalk02 and the two BudgetNight fixed comparisons
pass; the latter preserve readable road/pier lighting. FinalMetrics01 completes
the 133 m normal route, with wall/GPU/game-thread p95 24.561/23.338/7.147 ms,
53 active wood plants, 695 queried candidates and zero capacity misses. This
route reports only the existing motion-vector warning; GrassPalmWalk02 still
reports VSM marking capacity. TidewaterGrassFoliageRegression01 passes all 147 registered cases in 1731.05 s.
Its only two warnings are the known engine motion-vector console access and VSM
marking capacity. No test errors or reflected-class startup errors remain.
Continuous night routes and final visual/performance acceptance remain open.

## Lighting isolation — 2026-09-26

TidewaterForestLightingProbe01/02 are opt-in diagnostics in the same actual
133 m PIE route. They temporarily compare Lumen surface cache/indirect lighting,
hardware versus software tracing, and disabled diffuse GI. Probe02 also hides
canopy components briefly to isolate atmosphere and ground lighting, then restores
them; this is diagnostic evidence, never a proposed density reduction or an
acceptance image. Cleanup restores visibility, camera and Lumen settings even
when the latent command exits early. Normal production settings are unchanged.

With canopy absent the same view meters approximately EV100 11.7; looking up at
the sky meters 13.0. With dense canopy present the earlier meter was -4.17.
Atmosphere/sun render correctly in the unobstructed case. Software tracing and
turning GI off change the shadow color but do not repair the extreme balance.
This isolates the problem to dense-canopy shading/occlusion/light transport; it
does not yet identify one confirmed material or engine defect. No fake ambient
emission, arbitrary sunlight reduction or removed tree count is accepted as a fix.

## Nanite attribute compatibility correction (2026-09-26)

The native read-only audit confirms Plant meshes had Nanite disabled while the
Canopy family used it. An initial experiment enabled near Plant meshes, but
TidewaterPlantNaniteViews01 shows incorrect fern shapes and is **rejected** as
visual evidence. Engine installation files were only read, never modified:
Shaders/Shared/NaniteDefinitions.h defines NANITE_MAX_UVS=4, and
NaniteVertexFactory.ush aliases every higher UV index to UV3. The previous
six-UV adapter therefore cannot be used unmodified with Nanite. The limitation
also affects Canopy material variation, even though its wind used earlier UVs.

The project adapter now uses four UV sets: original surface UV, vegetation XY,
vegetation ZW, material age/variation. Source positions and all geometry/wind
float data stay unchanged. Exact material type and plant-kind bytes occupy vertex
color R/G; cosmetic material phase uses 16-bit BA (maximum error < 1/131070).
Every original triangle has uniform type/kind. No new trees or replacement
geometry are generated, and the original source buffers remain locked.

Blender writes color bytes inside their quantization bins because the legacy FBX
importer truncates normalized values. Native atomic reimport reads the existing
FbxStaticMeshImportData, so the importer explicitly changes its color option from
Ignore to Replace on guarded owned assets. Setting only the task import option
was insufficient: FoliageAttributeChecks01/02 caught all-white attributes and
failed; those reports remain. MeshDescription's linear color is correctly
converted back to its native vertex bytes for verification.

Two authoring attempts also remain documented: one used an invalid named RGB
output (the API uses an unnamed RGB output); another exceeded the plant shader's
interpolator path and failed compilation. Native vertex-color interpolation
already supplies the needed values, so the extra interpolator was removed.
Neither failure was accepted as a complete import. Authoring-20260926-223042
successfully builds both materials and reimports all eleven meshes.

FoliageAttributeBuild02 Development Editor Win64 succeeds in 8.23 s, with zero
compile/link errors. FoliageAttributeChecks03 passes all three selected cases:
34,152 native vertex-instance corners match canonical positions, eight float
attributes and four packed bytes; ten near meshes have built Nanite data with
area/semantic-UV preservation; the non-shadowing far palm remains conventional.
WoodCollisionData and MountainJungleData also pass. This checks imported data;
normal viewport/GPU deformation, new timing and full 151-case regression are
separate gates still in progress.

TransportProbe01 also compared full hit lighting, traversal budget, scene detail
and sky leaking in one fixed viewpoint. Extra indirect light brightened shadow
areas but did not fix bright sun speckles, so those temporary settings were all
restored and are not production changes. No Phase20.5c visual parity pass is claimed.

TidewaterPlantNaniteViews02 passes the original normal-movement route after the
successful attribute import. Matching waypoint 03/06 views and base-color/HDR
captures were inspected: fern and other plant shapes are restored. The HDR
forest meter is still very low (approximately EV -4.75) and sunlight speckles
remain overbright. The attribute/Nanite correction does **not** resolve or certify
daylight balance. All temporary diagnostic lighting overrides remain disabled.

TidewaterContactForestMetrics02 passes with 324 samples after four seconds of
warmup, 1600 x 900/Epic/100% screen resolution and no continuous readback:
wall median/p95 19.001/22.009 ms; GPU 17.987/20.449 ms; game thread 6.827/7.505 ms.
Peak wood count is 54, spatial candidates 685, capacity misses zero. Only the
known motion-vector console warning appears on this route. This is an integrated
comparison with the former 20.797/19.557 ms wall/GPU medians, not attribution of
all improvement to one change or a stable-60-FPS claim. Combined island streaming,
other viewpoints and visual parity remain open.

Current provenance validation: 143 generated files, 186 reviewed external files,
599 native packages and 364 locked Tidewater derivatives. Native assets were
authored through Unreal, never binary-patched; engine files were read only.

## Frozen integration result

The final Development Editor/Game builds pass (13.95/18.16 s), and complete
TidewaterFoliageFeedbackRegression01 passes **151/151** in 1755.99 s, zero errors
or missing/duplicate cases. Only the existing motion-vector warning is recorded;
there is no VSM marking warning in this particular full run. NativeAttributeLayout
and all historic physics/combat/wanted/persistence tests are green. Native asset
and binary hashes remain identical to the frozen pre-run snapshot. This closes
the current implementation regression gate, not the unresolved daylight balance,
combined-world performance or Phase20.5c visual parity gates.


## Further controlled lighting diagnostics (2026-09-26)

After the frozen 151-case checkpoint, opt-in diagnostics leave all production
lighting, source geometry, plant counts and physics unchanged:

- TidewaterSkyCaptureMeter01 completes the route, but its sky-illuminance display
  is black except overlays. It is rejected as a usable illuminance measurement.
- TidewaterNativeNaniteProbe01 accepts r.RayTracing.Nanite.Mode=1 in a fresh
  process. The normal native viewport and HDR capture retain clipped sunlight
  and an approximately -4.73 EV forest meter. Streamed Nanite tracing alone does
  not resolve the imbalance; the project setting remains unchanged.
- TidewaterSkyCaptureCompare01 compares real-time sky capture against a settled
  one-shot RecaptureSky in the same view, then restores the original mode. The
  recaptured case remains dark/clipped (approximately -4.78 EV). No production
  capture-mode change is accepted. ForestSkyProbeEditor01 compiles the opt-in
  harness successfully in 27.34 s, zero compile/link errors.
- TidewaterForestLightingProbe03 reruns the existing isolation after the four-UV
  Nanite correction. The inspected cache view now contains the nearby plants,
  without the earlier widespread yellow culled surfaces. Normal daylight still
  clips strongly. Temporarily hiding only the visible vegetation returns the
  forest-facing/sky-facing meters to approximately 11.7/13.0 EV, respectively.
  These hidden-canopy images are diagnostic only, never an accepted art change.

Each route reports Success, which certifies traversal and harness assertions,
not daylight quality or the cause of the lighting imbalance. The native HDR/cache
images remain under Saved/VisualOverhaul/<run>/UI. The single-view sky comparison
is available through RunPresentationChecks -Case JungleWalk -Capture
-ForestSkyCaptureProbe; it restores real-time capture on normal or early teardown.
No cached rendering setting is silently retained and no new gameplay is added.


TidewaterNoShortAO01 also passes the original trail, with a fresh-process short
range AO override. Its approximately -4.61 EV HDR result and clipped sun flecks
remain, so disabling AO is not accepted as a correction.

A view-independent cache-material candidate is now under review: the gameplay
raster pass retains the locked source's view-bent normals and grazing coverage;
the cache/ray pass uses source normals and leaf silhouette without the card
capture camera's direction. Palm bark detail also uses a fixed cache treatment.
TidewaterCacheSurfaceShaders01 passes five standalone DXC shader stages;
Authoring-20260926-233114 builds and saves the actual native materials without
shader errors, and ForestCacheProbeEditor01 passes in 6.82 s. Geometry, density,
wind, physical contact and color palettes are unchanged.

TidewaterForestTransportProbe02 still meters approximately -4.77 EV in its
baseline. Relaxing the pre-exposed probe/radiosity ray-intensity caps increases
indirect brightness but does not by itself repair the image. Its temporarily
higher scene detail also raises a cache-budget warning; all experimental values
are restored. No new production lighting-intensity limit has been accepted.
Sparse native-viewport stills (without continuous Draw/ReadPixels) and a fixed-EV
comparison are the next diagnostic controls; they are not performance benchmarks.


TidewaterForestExposureControl01 passes using sparse native HUD captures, without
continuous extra viewport draws or ReadPixels. Its baseline remains approximately
-4.89 EV, so removing continuous capture does not resolve the imbalance. A
transient fixed 9 EV range makes the forest very dark; it is not retained. The
experimental high scene-detail step can retain cache-page pressure until that
process ends, so later stages are diagnostic comparisons, not isolated
performance measurements. ForestExposureProbeEditor01 compiles in 22.05 s.

TidewaterSkyCaptureData01 passes in a separate sparse-capture process. One actual
sky/atmosphere pair is at world origin; the sky source is CapturedScene, intensity
0.9744 and distance threshold 150000 cm. After one-shot capture has settled for
16 seconds, the CPU SH0 RGB coefficients are **2620 / 4308 / 6288**, with a valid
processed sky texture. These coefficients are not lux measurements, but do rule
out an empty one-shot sky capture in this fixture. The sky data probe does not
establish the cause of dense-canopy occlusion. ForestSkyDataEditor01 passes in
6.64 s; normal daylight quality is still unaccepted.

ForestCacheMaterialChecks01 passes **4/4** focused cases: native mesh attributes,
wood collision data, real character/Chaos contact and mountain-jungle data. The
view-independent cache branch is retained as a material capture correction, not
as a claimed solution to dense daylight balance. No sunlight/exposure/GI limit
has changed in production. The prior full 151-case result belongs to the frozen
19fdb04 integration checkpoint; it is not relabeled as a full run of these later
material and opt-in diagnostic changes.


ForestCacheMaterialGame01 Development Game Win64 succeeds in 34.84 s, zero
compile/link errors; the final Editor build is ForestSkyDataEditor01. The sparse
ForestCacheNightPier01 route passes, and the inspected native 1600 x 900 pier
image retains readable wooden decking, lamps, village silhouettes and dense
hill canopy. This is the fixed comparison fixture, not the outstanding continuous
night voyage. Final provenance validation remains 143 generated files, 186
reviewed external files, 599 managed packages and 364 Tidewater derivatives.

## Bounded occlusion isolation (2026-09-27)

The optional `RunPresentationChecks -Case JungleWalk -Stills
-ForestOcclusionProbe` now runs seven fixed-view captures along the actual trail.
It separately forces the explicit post-process HitLighting mode, excludes only
near non-canopy plant components from GI, excludes only tree/shrub components from
GI, and restores the original flags. Render visibility, cast shadows, tree counts,
collision and spring/contact audio are never changed. RAII cleanup restores flags
and removes the transient post-process component on early exit as well.

TidewaterForestOcclusion01 is **not** an occlusion result: the initial runner
appended its option after launching UE, so only the ordinary trail ran. That
report remains intact. The corrected runner passes the option before launch and
requires the explicit completion/restoration marker. ForestOcclusionEditor02
compiles in 7.09 s; TidewaterForestOcclusion02 completes all stages and the trail
with player health unchanged. It records real HWRT enabled, HitLighting allowed,
and ForceOpaque=false. Each vegetation isolation changes ten near components.

The inspected explicit-HitLighting view raises shadow brightness (HDR scene
meter approximately -2.28 EV100) but still clips sun flecks. Removing canopy GI
brightens the view markedly; removing only non-canopy GI has a smaller effect.
These are causal diagnostics, not an acceptable rendering policy. Neither
excluding canopy lighting nor the expensive HitLighting mode is retained in
production. Daylight balance and full visual acceptance remain unresolved.

Engine source inspection also confirms that the distance-field/card builder
automatically classifies two-sided material triangles; a false forced-two-sided
build flag alone does not prove the current foliage representation is one-sided.
No blind reimport or engine modification was made on that assumption.

## Exposure and indirect-light clamp cross-check (2026-09-27)

The opt-in `-ForestExposureTransportProbe` records five native HDR captures at
trail waypoint six: baseline, raised probe/radiosity ray limits, fixed 9 EV with
raised limits, fixed 9 EV with original limits, then restoration. It cannot be
combined with another lighting probe. The transient post-process component and
both console values are cleaned up on completion or early teardown; the runner
requires the completion/restoration marker. No production lighting setting,
material, density, collision or contact response changes.

`TidewaterForestExposureTransport01` completes every stage and the existing
133 m trail in 112.94 seconds, with unchanged player health, 54 peak wood bodies,
685 nearby candidates and zero capacity misses. It has zero test errors and
the one existing motion-vector console warning. Development Editor Win64
`ForestExposureTransportEditor01` compiles successfully in 7.09 seconds.
`ForestExposureTransportGame01` Development Game Win64 also succeeds in
14.25 seconds, with zero compile or link errors. These editor-only diagnostic
changes do not replace the previously recorded complete 152-case regression;
the current 155-case registry has not received another full run.

Visual inspection rejects the combined exposure/clamp change as a repair:
raised limits under auto exposure brighten and wash out the forest while bright
sun flecks remain clipped; fixed 9 EV makes the forest nearly black with either
set of ray limits. The 100000 diagnostic ray limit is not adopted. Screens are
under `Saved/VisualOverhaul/TidewaterForestExposureTransport01/UI/`; these HDR
overlays are diagnostic evidence, not accepted art or performance measurements.
Dense-canopy daylight balance remains unresolved, separately from the passing
physical contact, traversal, audio and weapon/wood collision checks.
