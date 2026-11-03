# Phase 20.5 — South Beach visual reference

Status: authored assets integrated and technical verification passed. User art and
animation acceptance remains pending; this is not an overall art-quality sign-off.

## Scope and recovery

Started from `9c5bc3dd236e31623428e51074f81068c1a786be`, retaining Phase 20,
dynamic surfaces, swimming controls and helicopter search lights. Recovery tag:
`phase20-pre-visual-overhaul-20260923-9c5bc3d`; branch:
`codex/phase20-5-visual-overhaul`. The unrelated `Edit-FileText.ps1` is excluded.
No old checkout, engine edit, external upload, purchase or remote push is involved.

The work covers the existing 120 m South Beach road frontage and two existing
building plots, within the requested approximately 200 m reference area. It does
not extend the city, add missions, or replace vehicle/character/weapon models.

## Open and play

Run `Tools/Art/OpenVisualReview.ps1` from PowerShell (or use Run with PowerShell).
It opens the usual
`/Game/NeonCity/Maps/NeonCity_SouthBeach_Test` map. Press Play. This review launch
disables automatic persistence so the user's existing disk saves are preserved.
The default startup map and PlayerStart are unchanged.

* Coral Court: north side of coastal road, around **X=-4400, Y=2300 cm**.
  Retail arcade, recessed frames, awnings, apartment balconies, shutters and roof pergola.
* Seabreeze Works: **X=1400, Y=2400 cm**. Terracotta portal, tall glazing,
  bronze fins, deep sunshades, sawtooth roof and side screens.
* Original visitor hall entrance: **X=-1500, Y=1400 cm**. Approach within the
  existing interaction range, face the door and press **F**, then walk through.
* Shore route: follow the existing dock at **X=5200**, walk south from Y=-1000
  to the submerged steps near Y=-4400. WASD swims, Space ascends, C dives.
  Swim back to the steps to regain land movement. F boards the speedboat near its hull.
* Driving and night lighting use the existing sedan, vehicle controls and clock.

## Reproduce assets

Actual tools: UE **5.8.2**, CL **56702186**; Blender **5.2.2 LTS**,
build **d13f752e3b9c**. Editor scripting uses built-in PythonScriptPlugin,
EditorScriptingUtilities and a small project-only `NeonCityArtEditor` module.
IKRig is enabled; no retarget operation is required because animation is authored
directly on the exported project-owned Quinn/Mannequin reference skeleton.

Machine: Intel i7-13650HX, RTX 5060 Laptop GPU (8151 MiB, driver 592.15),
23.73 GiB RAM. Visual Studio 18.10.1, MSVC 14.51.36257 and Windows SDK
10.0.26100 are installed. UE reports the compiler as newer than its preferred
14.50 toolchain; both requested targets compile without errors. Disk space
checked during final verification: C 156.47 GiB / D 104.23 GiB free.

`Tools/Art/BuildArtPipeline.ps1` imports committed FBX/PNG files, creates a graph
only when absent and applies owned visual references. Parameters select the
Blender executable and EngineRoot; project paths derive from the script location.
Editor target must be compiled first to expose the editor helper.

Use `-RegenerateSources` only to regenerate this task's procedural source.
The fixed seed is **20523**. Generators preflight SHA-256 records before writing;
hand-edited source is preserved by failing explicitly. Import records in
`ArtSource/imported_assets.json` similarly protect manually modified UE packages.
The map/player checksums in `SouthBeachVisualOverrides.json` stop re-integration
after manual map/blueprint edits. Review and integrate those edits in the editor;
do not remove the checksums just to force a run through.

Artists can open `ArtSource/Buildings/SouthBeach/SouthBeachReferenceBlock.blend`.
Each named asset has its own collection; Coral Court is shown initially, others
can be unhidden. Geometry remains editable. Export a selected mesh using FBX,
metres, -Y forward/+Z up, modifiers, imported normals/tangents, no animation.
Keep its ground origin. UE is centimetres; do not multiply source scale by 100.
The importer verifies bounds. UE Python Rotator arguments are named explicitly;
the street-facing imported models use **yaw=180**, pitch/roll=0.

The first static probe measured **400×224.5×300 cm** after import. Its asymmetric
front key and the 2.4-second tread clip were displayed in rendered PIE before
the full import. Probe evidence is in `Saved/VisualOverhaul/ProbeUnderwater-20260923`.
Early trial captures at 700×330 and 882×462 are excluded from performance comparisons.

## Collision and rendering

Original map actors, collision, roads, doors, traffic routes, vehicle parameters,
task points and stable save IDs are retained. Replacement meshes have no generated
collision and placed visual actors use NoCollision. Original hidden coarse building
shells remain gameplay blockers; new upper floors/roofs are exterior scenery, not
new traversable interiors. This is a street-level reference, not a finished roof-access level.
All existing destructible actors and light-state relationships remain under their
original components. New decorative litter bins/signs are scenery, not new destructibles.

Hero meshes use Nanite; furniture has 100/50/20% LODs. Textures are 512, with only
road/concrete at 1024; base colour is sRGB, roughness and DirectX normals are linear.
Buildings use at most nine material slots. Paving currently uses repeated static
mesh actors; conversion to instances is deferred until a measured draw-call issue.

Coastal water uses the built-in single-layer-water output with Substrate conversion.
The direct Substrate water node in this UE build failed the ray-tracing shader
permutation; the supported legacy output avoids that fallback without editing
Engine shaders or disabling ray tracing. Cached lighting pre-exposure is 8 to cover
the existing physical daylight range. Night adaptation has a minimum EV100 of 2;
sun/street-light intensities and the clock remain unchanged. No added bloom or neon.

## Validation commands and evidence

* `Tools/Art/RunMotionValidation.ps1`: real rendered PIE, A swimming, B deck/reboard,
  C street/door/driving. Default captures consecutive 1600×900 JPEG frames at up to
  12 samples per game second plus CSV wall/game timestamps. `-NoCapture` checks
  assertions without screenshot overhead. Each sequence is continuous within its PIE session.
* `Tools/Art/RunVisualReview.ps1 -Label <unique>`: seven fixed camera views and actual
  swim pose samples; night exposure settles for 20 seconds. Logs resolution, quality,
  frame times, GPU frame cycles, graph assets and GPU board memory. The extra
  views cover the block outline, roof corner and bench detail.
* `-OriginalAppearance`: PIE-only restoration of recorded original visual actors
  and animation graph. It neither saves nor checks out the old map. This provides
  a matching-camera comparison with current capture settings; raw pre-change
  evidence also remains in `Baseline-20260923-1150`.
* Historical regression: `Tools/RunWaterLightingVerification.ps1 -SkipBuild`.

Final run paths, results and measured performance are recorded below.
No packaged-game performance claim is made. Continuous frames are evidence of the
actual run; motion smoothness, architectural character and animation polish require
human review. No promotional image or offline render is substituted for game captures.

## Actual verification history

The real pre-change run, `Saved/Automation/Aircraft-20260923-104051/Regression`,
passed 97/98. VehicleSurface.ParkedPIE failed the Pickup street-to-hood fixture;
its unchanged isolated rerun in `Saved/VisualOverhaul/BaselineParked` passed 1/1.
This pre-existing intermittent result is retained, not presented as a clean 98/98.

The first integrated full run, `Aircraft-20260923-123954/Regression`, passed
102/103. Water.SwimmingPIE found the shallow-dive intent defect described in
SwimmingAnimationValidation.md. The runtime was corrected and the original
assertions were retained. WaterLighting-20260923-125944 then passed all six water
checks and SearchLightPIE.

Final builds are in `Saved/Automation/WaterLighting-20260923-131039`:
Development Editor Win64 succeeded in 6.02 s; Development Game Win64 succeeded
in 23.39 s. There were zero compile/link errors. Existing Engine deprecated-API,
preferred-compiler and r.MotionVectorSimulation warnings remain; no Engine file
was modified. These are compilation/PIE results; this phase did not cook/package
or benchmark a packaged executable.

Final main regression: `Saved/Automation/Aircraft-20260923-131109/Regression`,
**103/103 Success**, zero assertion errors (98 existing IDs plus five new IDs).
Some IDs are explicitly gated diagnostics or cross-session entries; their actual
flagged runs are counted separately, not represented as 103 different gameplay routes.

The complete historical verification chain finished successfully:

| Report root under Saved/Automation | Groups | Success report entries |
|---|---|---:|
| Aircraft-20260923-131109 | Main 103, helicopter 5, marine 5, four A/B pairs 8 | 121 |
| FixedWing-20260923-131109 | Rendered 6, A/B pair 2 | 8 |
| VehicleCombat-20260923-131109 | Rendered 7, A/B pair 2 | 9 |
| Explosion-20260923-131109 | Rendered 16, A/B pair 2 | 18 |
| WaterLighting-20260923-131039 | Water 6, search light 1 | 7 |
| **Total** | **Seven actual process-A/process-B save pairs** | **163** |

Counting by exact `fullTestPath` yields **115 different report paths and 48 repeated
entries** in that chain. Twelve extra paths are parameterized A/B names for six
save families; explosion uses the same path in both processes. This is why neither
163 nor 115 should be called the number of independent gameplay features. The
main suite still contains 103 registered cases. All reports have zero assertion
errors. `Saved/VisualOverhaul/FinalRegressionSummary.json` preserves the exact rows.

Final visual reruns add eight Success entries: ReviewMotion-20260923 has Visual
5/5 plus Boat 1/1; ReviewBefore-20260923 and ReviewAfter-20260923 each execute the
flagged ReferenceReviewPIE once. The final acceptance set therefore contains
**171 Success report entries, 115 distinct exact paths and 56 repeated entries**,
with zero failed records. See `Saved/VisualOverhaul/FinalAcceptanceSummary.json`.
Intermediate trials and the two documented failed milestones are not silently
included in that final all-pass count.

## Final visual evidence and performance

All paths below are under `Saved/VisualOverhaul/` and contain this run's actual PIE
output. The final images include the corrected side glazing and tapered plant
blades; earlier ArtPass/FinalAfter/DeliveryMotion folders are development records.

* `ReviewMotion-20260923/Review.html`: local playback of all three continuous routes
  and seven matching-camera comparisons, with run labels and frame/time indicators.
* `ReviewMotion-20260923/A-Swimming.mp4`: shore, tread, swim, turn, dive, surface,
  shore and jump (493 original frames, 44.084 s recorded).
* `ReviewMotion-20260923/B-Boat.mp4`: moving deck, jump, swim back and F boarding
  (127 original frames, 11.245 s recorded).
* `ReviewMotion-20260923/C-Street.mp4`: close frontage walk, real F entrance,
  driving and night observation (477 original frames, 43.048 s recorded).
* `ReviewBefore-20260923/Street-01..07.png` and
  `ReviewAfter-20260923/Street-01..07.png`: matching views of apartment, arcade,
  commercial facade, night, whole frontage, roof corner and bench/planter detail.
* `Baseline-20260923-1150`: actual pre-modification captures and measurements.

The MP4s repeat the captured frames at their recorded game timestamps in a 30-fps
container. They do not invent frames: effective sampling is about 11 fps, at most
12 Hz. There is no interpolation, recolouring or offline scene render. The 1097
original 1600x900 JPEG frames and three frames.csv logs remain authoritative.
`Tools/Art/EncodeEvidence.py` uses Blender 5.2's VIDEO media type; invoke Blender
with `--python-exit-code 1` so a Python failure cannot appear as a successful export.

Measured on the machine above, DX12/SM6, Epic (all recorded sg.*=3), ray tracing
enabled, 1600x900, screen percentage 100. Each row samples 150 frames across 2.5 s
after settling, without continuous recording. Night settles for 20 s.

| View | Restored original GPU mean ms | Final GPU mean ms | Final GPU p95 ms |
|---|---:|---:|---:|
| Apartment | 6.721 | 7.987 | 8.317 |
| Arcade detail | 7.026 | 8.081 | 8.253 |
| Commercial | 7.082 | 7.930 | 8.212 |
| Night | 6.285 | 7.419 | 7.642 |
| Frontage overview | 7.522 | 7.825 | 8.114 |
| Roof corner | 7.016 | 7.741 | 7.954 |
| Bench/planter | 6.679 | 7.078 | 7.274 |

Across these seven equally sampled views, GPU mean increased from 6.90 to 7.72 ms.
Whole editor/PIE wall-frame means remain about 16.67 ms (final p95 18.04–18.43 ms).
That approximately 60-Hz editor cadence masks spare GPU capacity; it is not proof
that the artwork has no rendering cost or that a packaged game has the same fps.
Raw baseline wall means were 16.658–16.670 ms at the same 1600x900 resolution.

GPU-board memory peaks: original pre-change run 4597 MiB; restored-old appearance
5758 MiB; final appearance 5680 MiB of 8151 MiB. These are whole-board samples
including editor/other allocations. Restoring old visibility still loads new
packages, and final reviews include three extra views, so the latter two numbers
are not evidence of an asset-memory optimization. Final logs contain no material
fallback or video-memory-exhaustion warning.

## Acceptance boundaries

No paid/licence-dependent material is blocked; no external asset was acquired.
Both hero buildings, furniture and five real swimming cycles are delivered with
editable sources. The surrounding greybox, opaque glazing/no shop interiors,
upper-floor/roof traversal, richer lived-in detail and underwater presentation
are not a finished production environment. Swimming shoulder deformation,
frog-kick timing and entry/exit blends need human review; dedicated entry dive,
climb-out and water-death clips are not supplied. Technical tests do not approve
those artistic decisions. No Phase 21 work is started.

## Phase 20.5c environment intake checkpoint

The preceding no-external-assets statement describes the earlier phase. The
current tranche adopts CC0 Poly Haven asphalt, fine beach sand and a modified
hydrant, with receipts in `ArtAssetManifest.md`. A different coastal sand source
was rejected because its algae/gravel appearance did not match this beach.

`Saved/VerticalSlice/LicensedEnvironment02` contains nine real UE PIE images:
Road/Sand/Hydrant at 10:00, 17:18 and 21:00, 1600x900, Epic, 100% resolution.
All nine were inspected. The shore triangle intersections visible in the first
attempt are resolved. Night surfaces remain readable under existing lighting;
no material fallback appeared. The local environment test passed. Editor and
Game targets compiled without compile/link errors for this tranche.

These views also retain unfinished surrounding artwork: road markings have a
raised geometry appearance, pavement tiling is conspicuous, daylight exposure
is bright, and the surrounding vehicles/characters still require their planned
passes. These are not final visual acceptance images. The full `VerticalSliceEnvironmentRegression01` run completed 114 unique tests:
113 passed, with one fixed-wing building-impact assertion failure. The unchanged
`FixedWing.DamageWaterPIE` passed its isolated repeat in
`Saved/VerticalSlice/FixedWingEnvironmentRecheck01` (one repeated execution).
This is an intermittent concern, not a claimed clean full-suite run; it remains
on the next integration gate. Long-session performance, rain captures and normal
continuous gameplay acceptance remain pending.

## Phase 20.5c occupant contact checkpoint

The shared seated pose now uses the reviewed CC0 Driving_Loop with standard
non-stretch two-bone IK. Eight editable derivatives of owned transport artwork
correct control reach, seat height, fixed-wing footwell and FBX handedness.
They keep the existing physics bodies, mass, inputs, IDs and weapon mounts.

`TransportOccupants04` passes normal F entry/exit and repeat-entry checks for
seven player transport classes. Its actual UE pose captures were individually
inspected; major unreachable-control/head-clearance issues are resolved. These
images still show unfinished exterior/glazing quality and fine finger contact.
See TransportOccupantValidation.md for the build and full-regression evidence.
No entry animation, final vehicle artwork or complete transport loop is claimed.

## Phase 20.5c reviewed pistol checkpoint

The existing pistol now uses a modified CC0 base with a compact original finish,
palm-aligned grip and socket-driven left-hand contact. ReviewedPistolPresentation03
checks actual bone/socket distance and the live muzzle origin. Its real aim/reload
frames 21, 32 and 43 were inspected; individual finger posing remains unfinished.
Editor/Game Development builds and all 116 unique tests pass for this checkpoint.
ReviewedPistolValidation.md records source adaptation, diagnostics and limits;
the full four-weapon art and character experience are not marked complete.

## Phase 20.5c marine feedback checkpoint

MarineThrottleLoop05 supplies actual driving/swimming/reboarding evidence with
progressive throttle, a local HUD, spatial motor playback and bounded surface
wake/spray. The rejected Loop04 puff-like wake is retained; the revised trail
overlaps lengthwise behind the hull. The real frame 161 and exit-feedback Slate
image were inspected. The separately captured world is 1600x900, while the Slate
client image is 1264x711; this is not final composited performance acceptance.

The motor recording and particle mask passed author-page CC0 checks before
download and have raw-source receipts. Engine audio start/fade is tested, but
audible quality is pending. Editor/Game builds, 12 focused cases, two-process
marine persistence and all 117 registered tests pass. See MarineExperienceValidation
for exact evidence, repetitions, measured short-route timings and unfinished
boat/ocean/yacht artwork. No full marine or whole-slice completion is claimed.

## Phase 20.5c pilot instruments (in progress)

AircraftPowerLoop01 passes an actual input route with cruise 65.58 km/h, fast
flight 104.71 km/h, gradual power, controlled slowing, hover, landing and exit.
Its inspected white HUD lacked contrast over sand. The revised shared dark
backing is inspected in AircraftInstrumentsMissile01: real lock acquisition,
confirmed/lost state and ammunition after launch remain legible. World and Slate
captures retain the disclosed 1600x900 versus 1264x711 size difference.

Editor/Game Development builds pass. AircraftPowerFocused01 passes 25 unique
focused cases; AircraftPowerCrossSession01 restores matching persistent aircraft
state in two independent processes. AircraftPowerFullRegression01 passes all 118
unique registered cases in one queue. This is functional regression evidence;
overlapping Blender authoring renders mean it is not a final performance run.
HelicopterExperienceValidation retains the limitations; these routes do not
replace final uninterrupted normal-game weapons, night-light and audio acceptance.

## Phase 20.5c aircraft lighting checkpoint

Poly Haven's CC0 Portable Searchlight optical head is separately adapted into an
unbranded NeonCity aircraft fitting. Housing and emitter sockets keep the actual
spotlight attached through bounded aim. A thin authored coastal medium supports
real night scattering with distance/shadow limits. Road, passing car, beach and
ocean views were inspected in AircraftSearchlight08; L input, firing while lit,
physical return, landing and exit pass. Overexposed sand and excess street-lamp
scattering from earlier attempts were corrected, retaining the failed evidence.

The optional test window now measures 1600 x 900 for both world and HUD. Earlier
1264 x 711 HUD captures are historical evidence with a known limitation; they are
not retroactively represented as native 1600 x 900 output. Nine environment views
and the marine input route pass with the new map/window. Editor/Game builds pass;
the short no-readback lighting performance run is recorded in
AircraftLightingValidation. AircraftLightingFullRegression01 passes all 118 unique
registered cases, without omissions, unexpected entries or duplicate names. This does not
close the overall aircraft art, audio or uninterrupted Section 21 acceptance.

## Phase 20.5c navigation checkpoint

The local vector minimap uses the existing authored road lanes, stable mission
destinations and the currently possessed pawn. MarcusNavigation01 completes the
ordinary home-to-car-to-delivery route without staged vehicle transforms, pays
$350 once and verifies save/load. The four actual HUD views were inspected; the
reward backing was increased to fit both lines. MarcusNavigationCrossSession01
repeats the physical route and restores matching car/cash/contract state in a new
process. Its completion/restart views confirm the revised layout. Editor/Game
builds and NavigationFullRegression01 pass all 122 unique registered cases.
Phone-open V
no longer also arms a vehicle. NavigationExperience records the failures, state
fault injection and verification limits. This is incremental UI/flow work, not
the final world art, opportunity system or whole-game acceptance.

## Phase 20.5c opportunity choice UI (in progress)

The phone now presents browsable invitations/history and explicit accept,
decline and later choices. OpportunityChoices01/02 pass and their actual 1600 x
900 available/later/declined/history views were inspected, as were both long-
message scroll views. The closing hint remains visible. OpportunityCrossSession01
retains ignored and declined decisions in independent processes. A historical
recreated-contract regression was found and fixed without weakening its assertion;
the focused repeat passes. OpportunityMarcus01 still completes the ordinary
physical delivery and its final offer/reward views were inspected. Editor/Game
builds pass; the complete 123-case regression passes with zero errors. These are functional screens, not final phone art or
completion of the wider city opportunity activities.

The Bank phone increment uses the existing scroll panel and actual input context.
BankingPhone01's six real 1600 x 900 images were individually inspected: balances,
terms before confirmation, payments, restored history and all-funds transfer are
readable; long history scrolls and the closing hint remains fixed. Banking adds
no external art, native-app screenshots or fabricated gameplay images. This is
functional UI verification, not completion of the final phone art/audio stream.

BankingFullRegression01 passes all 126 registered cases with zero errors; the
motion-vector and non-Nanite VSM marking overflow warnings remain visible in the
evidence. No frame-rate or final visual acceptance is inferred from this queue.
