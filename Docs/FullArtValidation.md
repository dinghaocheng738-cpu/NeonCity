# Phase 20.5b review record

Work starts at verified `63c0599ee79784dd1a94ce6280a1428ee67bdfe0`.
Rollback tag: `phase20-5b-before-art-20260923`.
Working branch: `codex/phase20-5b-full-art`.
The pre-existing untracked `Edit-FileText.ps1` is outside this change.

Toolchain: Blender 5.2.2 LTS and Unreal Engine 5.8.2, Win64. Original editable
source lives in ArtSource/FullArt and ArtSource/EnvironmentFullArt. All package
writes use UE factories/editor APIs; no Engine installation files are modified.

## Reproduction

Close the editor after saving user edits. Run `Tools/Art/OpenFullArtReview.ps1`,
press Play, then PageDown/PageUp for the fifteen numbered stops and T for time.
Exit/land before changing stops. This opt-in session disables automatic disk
persistence and grants a temporary review wallet/catalogue; it does not change
ordinary defaults. Do not manually save this review session to a personal slot.
Yacht is a non-interactive visual shell. The runway and target range are existing
opt-in test fixtures, not a newly built airport.

Helicopter: F enter, Space climb, V arm, Tab twice selects right missile mount,
RMB locks a valid visible vehicle, LMB fires. The existing Phase 19 dwell, LOS,
ammo, turn rate and damage rules remain authoritative. The normal helicopter now
installs that existing loadout in BeginPlay; initial state is SAFE.

`RunFullArtReview.ps1 -Label <unique>` runs art assertions and rendered day/sunset/
night stills. `-Motion` additionally records the real-input missile sequence;
`-MissileOnly` isolates that test. Still images do not prove animation or flight.
`RunFullArtMotion.ps1 -Label <unique>` captures swimming, boat deck/reboard,
street driving, helicopter/fixed-wing flight, pistol/long-gun input, the ordinary
helicopter missile sequence and shore walk/swim/return in separate UE processes.
`RunFullArtRegression.ps1 -Label <unique>` queues every registered NeonCity test
and records CPU/GPU samples without continuous pixel readback. The optional
review and tour tests have separate opt-in runs; a skipped opt-in body is not
reported as executed visual verification.
Frames retain actual game/wall timestamps; video containers must not be presented
as native 30 fps when the source capture cadence is approximately 12 Hz.

## Visual quality gate — NOT COMPLETE

The first in-game pass exposed visible segmented arch trim, over-transparent car
glass, an inappropriate stucco normal on transport finishes, and obstructed static
review cameras. These prompted source/material/camera corrections. Keep earlier
evidence as iteration history; do not substitute it for final capture.

**NOT COMPLETE after in-game still review:** a modern, fully resolved vehicle
design language; distinct physical pickup wheelbase/SUV ride height/sports roof;
production vehicle interiors; final aircraft cockpit/engine detailing; final
character clothing/meshes; bespoke swimming entry/climb-out clips. Shared chassis
dimensions and Quinn compatibility are intentional constraints, not fulfilled
versions of those requests. No purchased or downloaded replacement assets were
used to conceal these limits.

The latest still set is `Saved/FullArt/FinalArt02`: fifteen views at 10:00,
17:18 and 21:00, 45 original 1600×900 PNGs. All three lighting variants were
inspected, including the revised aircraft cockpit geometry. This run's shore
test failed; the stills remain valid art evidence, not a claim that its complete
test suite passed. Subsequent focused runs and the complete final regression
are recorded separately below.

The following is the actual art assessment, not an automatic-test grade:

| Requested category | Delivered in this change | Remaining quality gap |
|---|---|---|
| Coral Court / Seabreeze Works | Roof condensers, drainage pipes, collars, service cabinets added to prior facades | Rear elevations/corner transitions and storefront interiors remain schematic; a broad facade redesign was not completed |
| Sedan | Wheel arches, glazing/seals, mirrors, seams/handles, lamps, four separate wheel instances, seat/dashboard/steering shells | Angular retro silhouette, simplified nose/tail and uneven body shading; not yet the requested resolved modern sedan |
| Sports | Wider body, sports trim/spoiler and wheel finish | Roof, tire radius and chassis remain shared; silhouette is insufficiently different from sedan |
| SUV | Wagon cabin/roof rails and utility wheel finish | Shared ride height and modest cabin difference; not a fully resolved SUV proportion |
| Pickup | Separate cab and open cargo bed | Original physical wheelbase preserved; requested longer wheelbase is not fulfilled |
| Police | Original NeonCity lettering/livery, lightbar, pushbar | No real police logo; night body readability poor away from street lights; no complete emergency-light animation pass |
| Speedboat | Shaped hull/bow, deck, windshield, seats, helm and navigation-light surfaces | Simple cockpit/engine shapes; high-quality marine detailing and wake presentation remain incomplete |
| Yacht | Hull, decks, cabin exterior, rails, window band and swim platform | Boxy cabin shell; no boarding, collision or interior gameplay and no finished interior |
| Helicopter | Fuselage, glazed cockpit/frames, seats, rotor/tail rotor, skids, engine housing, existing mounts | Improved windshield topology; airframe/door/engine detailing and night legibility remain unfinished |
| Fixed wing | Fuselage, wing thickness, gear/tail, cut cockpit opening and canopy, engine details | Simplified airframe, seat/cowl and wing shading; no production cockpit; night runway review is almost black |
| Four firearms | Distinct barrel/stock/magazine variants, grip/trigger area, barrel-aligned muzzle locations | Basic modeled finish; actual upper-body hold/aim/reload animation remains unarmed-template based and needs a dedicated Quinn-compatible art pass |
| Player/NPC | Five stable-ID selected coastal material palettes; reduced metallic response | Existing Quinn mesh retained; palette is not new clothing or a finished human character |
| Swimming | Five authored skeletal loops refined at clavicle/arm/leg/hip paths | Entry/climb-out clips and polished shoulder deformation are not complete; continuity is reviewed from recorded frames separately |
| Beach / shore / ocean | Sloped sand, dry/wet material transition, foam interface, limited furniture/palms and submerged floor extension | Sand reads too coarse/rock-like, palm silhouette schematic, shoreline edge visibly finite; ocean distance/underwater presentation not fully upgraded |
| Roads / props | Covers/drains, repair strips, hydrants, utilities, parking meters; breakable lamp/bin visual adapters | Existing lane/curb geometry retained; broad worn-road treatment and unified background set unfinished |
| Day / sunset / night | All 45 views captured with real world time/lighting | Day/sunset readable; offshore and aircraft/police fixture night views lack sufficient ambient illumination. Not fixed with bloom or image brightening |

These are authored prototype upgrades and a reversible integration, not a passed
Full Art Pass. Completing the art gate requires coherent vehicle/airframe/boat
design work, proper surface topology/textures, Quinn weapon animation assets,
natural sand/vegetation/water treatment and night-lighting calibration. Current
procedural geometry cannot be relabelled as those finished assets.

## Compatibility corrections and iteration record

Old primitive components, templates, physics bodies and assets remain available.
No Engine installation edits, downloads, purchases or new city region were made.
The existing helicopter loadout is installed on the ordinary map helicopter;
its inputs, locks, projectile pool, damage and guidance are the existing Phase 19
systems. No second missile framework was created.

The shore skirt is the only new gameplay collision: a static floor matching the
visible slope, excluding the established dock lane. FinalArt01 exposed its
submerged end wall on return, so the authored floor was extended down to about
Z=-410 cm. FinalArt02 exposed that the test's Y=-3620 turn point was no longer
deep water. Moving the checkpoint to Y=-4650 then exposed a real wading gap:
the character fell to Z=-539 instead of swimming after it had entered the same
water volume while shallow. SwimmingComponent now switches grounded, able,
submerged characters with no shallow support back to native Swimming. No
transform assignment, new trace or modification of Epic movement was introduced.

`ShoreWadingFinal01` passed the complete walk → swim → walk route, with the deep
checkpoint at Z=-188.498 cm / Swimming, and also passed the real-input helicopter
missile test. Its ReviewPIE used AuditOnly (no new images); TourPIE was opt-out.
The successful ten registered results must not be called ten visual scenarios.

## Source, provenance and scope

41 new static meshes, 24 shared materials, 65 new UE packages; five existing swim
animation packages updated. The map has 57 additional tagged placements. Eleven
existing SouthBeach static-mesh packages were re-saved by the original importer;
they are not eleven newly authored models. Source inventory totals 242,224 unique
LOD0 triangles, not a visible frame count. The SHA-256 validator checks 100
generated source files and 131 managed UE packages plus integration ledgers.

The editable Blender sources, generators, FBX files and import manifests are in
ArtSource and Tools/Art. New geometry and graphs are original NeonCity work;
existing Quinn skeleton/mesh/materials remain Epic template content. No real
manufacturer or police branding was copied. See ArtAssetManifest for the complete
source/licence table and FullArtInventory for per-mesh budgets.

No Phase 21 expansion, new aircraft/weapon class, submarine, torpedo, fishing or
aquaculture gameplay is included. Future marine requirements are documentation
only. No packaged/cooked build or multiplayer validation is claimed.

## Historical and cross-session validation

The September 23 complete verification chain passed 172 test executions across
22 reports (zero failed). This includes 112 then-registered tests in
`Saved/Automation/Aircraft-20260923-174528/Regression`, five aircraft evidence
cases, five marine evidence cases, six fixed-wing, seven vehicle-combat, sixteen
explosion, six water and one search-light case. Repeated executions are not 172
unique tests. The ordinary compiler/runtime warnings remain recorded in logs.

Seven save A/B pairs ran in genuinely separate OS processes: general persistence,
weapons, marine, helicopter, fixed wing, vehicle combat and explosion/destruction.
The chain verified matching save IDs and restored state. Its report roots are
`Aircraft-20260923-174528`, `FixedWing-20260923-174528`,
`VehicleCombat-20260923-174528`, `Explosion-20260923-174528` and
`WaterLighting-20260923-174458` under Saved/Automation. This chain predates the
last cockpit/shore refinement; the final complete queue and motion routes below
verify the subsequent state. Save schema and IDs did not change.

The fifteen-stop opt-in tour actually ran in
`Saved/FullArt/TourFinal01/TestReport` (1/1). No tour functionality is inferred
from the opt-out TourPIE entry in the general queue. The normal editor launcher
is supplied for user review, which remains pending.

## Final complete queue (September 25)

`Saved/VisualOverhaul/FinalPerfRegression01/Regression/index.json`: **113/113
registered results successful, 0 failed, 0 omitted from the previous 112-case
queue**, 1124.80 seconds. One result carries the existing engine
`r.MotionVectorSimulation` render-thread warning. Opt-in review and cross-session
bodies still require their separately flagged evidence; see the records above.

Coverage includes ordinary Third Person movement/door interaction, four road
vehicle profiles, collision/damage/ownership/persistence, moving and parked roofs,
police chase/roadblock/decay, marine buoyancy/deck/reboard, aircraft flight/water/
searchlight, fixed-wing takeoff/stall/landing, vehicle combat, character weapons,
props/glass/light destruction, crime/wanted, economy/phone, property and saves.
The new shore route and normal-helicopter input route also pass in this queue.

## Final performance samples (no continuous image readback)

Hardware: RTX 5060 Laptop 8151 MiB, i7-13650HX, 24 GB system RAM; DX12/SM6,
1600×900, Epic, 100% render scale. Samples below come from selected moving-game
stages in FinalPerfRegression01, with no simultaneous Blender/other UE run.
Times are milliseconds; median / p95. CPU columns are measured game and render
threads, not their sum. Editor wall-frame pacing includes editor scheduling.

| Scenario | Samples | GPU | CPU game thread | CPU render thread | Wall frame |
|---|---:|---:|---:|---:|---:|
| Walking reference block | 173 | 8.89 / 9.45 | 7.93 / 9.44 | 6.48 / 7.83 | 16.63 / 18.38 |
| Driving among the new vehicle appearances | 45 | 8.63 / 8.89 | 8.12 / 9.23 | 6.26 / 7.32 | 16.28 / 17.68 |
| Beach/water swimming | 140 | 6.38 / 7.61 | 7.31 / 7.75 | 5.87 / 6.34 | 16.70 / 17.61 |
| Helicopter hover/city flight | 144 | 7.05 / 8.59 | 7.04 / 7.29 | 5.75 / 5.92 | 16.66 / 17.64 |
| Fixed-wing takeoff/bank | 117 | 7.52 / 9.00 | 7.27 / 7.54 | 5.78 / 5.98 | 16.58 / 17.66 |
| New shore walk/swim/return | 144 | 8.95 / 11.53 | 7.35 / 7.62 | 5.78 / 6.08 | 16.69 / 17.49 |

The roughly 60-Hz editor cadence is not an uncapped packaged-game FPS result.
VRAM is sampled board-wide by nvidia-smi, including editor/other allocations;
it is not a per-asset memory delta. This regression's board median/peak was
5579 / 5800 MiB. FullArt02's separate 45-view review peaked at
6045 MiB. Its near-zero render-thread counters are not used as CPU measurements.
Raw per-stage CSVs and PerformanceSummary.json live inside the final regression
folder; selected scenario/stage definitions are in
Saved/FullArt/FinalScenarioMetrics.json. The seven identical reference cameras
are compared separately, rather than comparing these gameplay samples with a
different baseline camera.

The same seven reference cameras compare the final Phase 20.5 capture
`ReviewAfter-20260923` with `FullReferenceFinal01` (September 25). Both use
150 GPU samples per camera, the same 1600×900/Epic/100% settings and hardware.
The equally weighted mean of camera GPU means rose from **7.723 to 8.047 ms
(+4.19%)**. This is an observed cross-run difference, not an isolated asset-only
cost measurement or a packaged-game benchmark.

| Camera | Before GPU mean / p95 ms | After GPU mean / p95 ms |
|---|---:|---:|
| Street-01 | 7.987 / 8.317 | 8.310 / 8.644 |
| Street-02 | 8.081 / 8.253 | 8.200 / 8.398 |
| Street-03 | 7.930 / 8.212 | 8.079 / 8.305 |
| Street-04 | 7.419 / 7.642 | 7.706 / 7.972 |
| Street-05 | 7.825 / 8.114 | 8.640 / 8.948 |
| Street-06 | 7.741 / 7.954 | 7.943 / 8.172 |
| Street-07 | 7.078 / 7.274 | 7.450 / 7.947 |

Both reference runs retain about 16.67 ms mean wall cadence. Final per-camera
wall p95 is 17.605–17.987 ms. Board-wide peak VRAM was 5680 → 5805 MiB;
these include editor/other allocations, so 125 MiB is not asserted as new asset
memory. Final reference PIE passed 1/1 with no material compile/fallback error.
CPU moving-scenario measurements are in the preceding table.

## Final continuous PIE evidence

`Saved/VisualOverhaul/FullMotionFinal01` completed eight isolated rendered UE
processes: 12 registered results successful, zero failed. Nine sequences contain
2,746 original 1600×900 JPEG frames with game time, wall time, stage, pawn and
movement/position CSVs. Each sequence comes from its own uninterrupted scenario;
sequences from separate processes are not represented as a single play session.

| Sequence | Frames | Actual route |
|---|---:|---|
| A-Swimming | 473 | Tread, forward, turn, dive, underwater, surface, shore return |
| B-Boat | 124 | Moving deck, jump, swim, reboard |
| C-Street | 460 | Walking, door, vehicle approach, driving, night |
| D-HelicopterMissile | 101 | Normal helicopter, F/V/Tab/RMB/LMB input, lock/occlusion/reacquire/fire/hit |
| E-HelicopterFlight | 393 | Takeoff, hover, forward/strafe/yaw, landing and exit |
| F-FixedWingFlight | 853 | Player approach, takeoff, bank and existing flight test route |
| G-PistolAimFire | 106 | Equip, aim camera, shot, target HP change, reload/holster |
| H-SMGRifleReload | 98 | Existing SMG/rifle selection, automatic fire and reload input route |
| I-ShoreWalkSwimReturn | 138 | Walk down authored sand, swim beyond submerged floor, walk back |

Representative frames at different times/stages were inspected from all nine
sequences. Swimming has real limb motion, but shoulder/hip articulation remains
stiff and the underwater backdrop/floor edge is visibly unfinished. The boat's
rear engine cover occludes the character's feet from the rear camera; that image
alone does not establish mesh penetration. Deck/reboard assertions pass.

The gun footage confirms a substantial visual failure: the ordinary unarmed
pose remains during aim/fire/reload, and the gun follows the lowered hand rather
than a proper firing posture. Muzzle coordinates, hit detection, recoil and ammo
assertions pass; **weapon animation / visual aim alignment is NOT COMPLETE**.
The fixed-wing route shows actual flight/bank but also the finite grey test-area
boundary. Flight correctness does not approve that environment art.

The helicopter missile route uses the ordinary map helicopter's installed Phase
19 loadout, not a second combat implementation. After one-time fixture staging,
keys drive possession, arming, mount selection, lock request and launch. Assertions
verify Searching → Locking → Locked, blocked LOS → Lost, no occluded firing,
reacquisition dwell, one round consumed, launch within 450 cm of the actual right
mount, bounded 45°/s tracking and target HP reduction. Recorded frames 80–85 show
the projectile leaving the right mount and travelling toward the car. The test
aims the existing camera at a staged target; this is automated real-input PIE,
not a claim of manual human play or finished explosion VFX.

Evidence is local under ignored Saved folders, not a repository media payload.
Generators, test routes and manifests are committed so it can be reproduced.
The review viewer preserves raw frames without interpolation or enhancement;
capture cadence (about 12 Hz) is not game rendering FPS. Technical Validation
and User Visual Approval remain separate.

Open `Saved/VisualOverhaul/FullMotionFinal01/Review.html` for the sequence player,
45 lighting stills and seven before/after pairs. Frame inventory verification
checked all 2,746 JPEG dimensions, exact frame/CSV correspondence, ascending
timestamps and all 59 still/reference PNG dimensions; results are in
`Saved/FullArt/FinalEvidenceInventory.json`. The frame files remain unmodified.

## Final build and repository checks

Development Game Win64 succeeded on September 25 in 31.91 s, including the final
wading fix and link of NeonCity.exe (`Saved/FullArt/GameFinalSep25.log`): zero
compile errors and zero link errors. Development Editor Win64 had already
compiled this source successfully in `EditorShoreWadingFix.log` (22.56 s), before
the complete final queue and motion runs. `EditorFinalSep25.log` records the
final incremental verification: Succeeded / target up to date, 1.20 s. Both
Win64 Development targets have zero compile and link errors.

Warnings remain: MSVC 14.51.36257 is newer than UE's preferred 14.50.35717;
engine headers emit deprecated-API C4996 warnings; rendered automation can emit
the existing r.MotionVectorSimulation thread-safety warning. These are not
reported as zero warnings. No engine-installation file was changed to suppress
them. A packaged/cooked shipping build, multiplayer and long-duration gameplay
soak remain unverified.

The final source/package hash check passes: 100 generated files, 131 managed
packages and matching map/player integration checksums. All 24 Python authoring
scripts parse, all 12 PowerShell scripts parse, and git diff --check passes.
The scoped change contains 202 files, including 65 new and 17 modified Content
packages (the latter includes the map); no Saved evidence or generated build
output is staged. The user's unrelated
Edit-FileText.ps1 stays untracked and unchanged (SHA-256
2513B82BB9AF8DE6A2EC1FE4DC22D69EE56B16B550A13019473F97BF6265E1EA).
The rollback tag and original visual components remain available. Technical
integration passes the recorded checks; **Full Art Pass: NOT COMPLETE** and
**User Visual Approval: PENDING**. Stop here; Phase 21 is not started.
