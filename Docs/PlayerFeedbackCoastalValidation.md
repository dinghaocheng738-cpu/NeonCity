# Player feedback coastal integration checkpoint

2026-09-27. Governing checklist: `PlayerFeedbackRegister.md`, PF-001–PF-035.
**Overall vertical slice: FAIL. This is an integration checkpoint, not acceptance.**

## Root cause and changes

Normal South Beach never loaded the existing island map. Native authored-map
audit and a deliberately failing normal-start PIE case found one level and zero
islands. The separate island tests did not cover this gap. World Partition was
false; this was not a culling or missing partition-cell defect.

The existing Landscape, village and forest now stream into the actual mainland
world. Original geometry, assets, movement, buoyancy and physics remain in place.
Native UE authoring saved both maps; receipts retain before/after hashes and all
545 original mainland actors. The existing water surface/provider now spans the
crossing. The fishing boat's stable ID now uses the canonical island berth.
The marina bench and its paired collision moved together to clear the real steps.
The obsolete floating instruction sign is suppressed in the connected world.

Save restoration waits for initial region readiness and explicitly reloads
destination collision before validating an island position. Legacy untouched
prototype berth records migrate conservatively; deliberately moved boats retain
their chosen locations, ownership and condition.

## New native checks

| Evidence run | Result | What it establishes / limit |
|---|---|---|
| PlayerFeedbackWorldAudit01 | Native read-only audit complete | Existing island was only in its own map |
| PF007NormalWorldBefore01 | FAIL, retained | Correct reproduction: no island in ordinary startup |
| PF007ConnectedShore02 | PASS | Actual home spawn, F exit, ordinary walk to pier, real terrain sightline; image reviewed |
| PF006IslandBerth01 | FAIL, retained | Fixture incorrectly began outside a pier rail; no collision removed |
| PF006IslandBerth02 | FAIL, retained | Swimmer remained submerged after stepping off the high pier; existing ascend input was required |
| PF006IslandBerth03 | PASS | Real default berth, native buoyancy/mass, swim/F boarding, helm, camera, drive, moving deck and deck fishing; initial character placement is a declared fixture |
| PF009CoastalVoyage01 | FAIL, retained | Automated pilot steered into the mainland fender before clearing its berth |
| PF009CoastalVoyage02 | FAIL, retained | East-side test route crossed actual shallow island terrain |
| PF009CoastalVoyage03 | PASS, 330.53 seconds | Continuous ordinary home/F/walk/F/WASD/Space route around west-side deep water, arrival outside island pier, stopped-vessel F exit; no fixture relocation |
| PF009CoastalRestore01 | PASS | Real test-slot disk save on island pier, unload region, load, correct floor/location retained, dedicated test slot deleted |
| PlayerFeedbackCoastFull01 | PASS: 158/158, zero errors | All 155 historical registrations plus three new coastal checks; no missing/duplicate/unexpected registrations |
| PlayerFeedbackCoastMarineRestart01 | PASS: independent Session A/B | Processes 14016 and 6784 restored identical stable vehicle/save IDs, transform, condition 0.5 and ownership false |
| PlayerFeedbackCoastMaterialRecovery01 | PASS: 5/5, zero errors | Final-code island reload/material parents, normal entry, village walk, fishing boat loop and world lighting budget |

The voyage's west passage was checked against the locked source heightfield:
sampled segments with 20 m lateral margin have at least 4.3 m depth. This guides
the automated driver only. It changes neither the seabed nor the vessel's physics.
The normal day/night clock kept running: departure was daylight, arrival night.

The full queue completed in 2,154.12 test seconds: 155 successes plus three
successes with warnings, zero failed/not-run cases. Its numerical post-checks also
passed: 243 FFT comparisons, 704 water-query points and 28,128 source/native GPU
surface vertices. Those numerical checks still do not establish live ocean motion.

Reviewing warnings exposed a real island-reload defect despite those assertions
passing: the village's repeated BeginPlay created a new dynamic material with an
existing dynamic material as parent. That parent type is invalid in UE. The village
now reuses its instances and resets its bounded working list. CoastalRestorePIE
additionally verifies all 31 village visual parts retain a valid authored parent
after the region reload. The 158-case run predates this small correction; the
following focused run and rebuilt targets validate the final change explicitly.
All five follow-up cases pass in 128.95 seconds. The invalid material-parent
warnings are gone. Only the known engine motion-vector warning appears in that
follow-up (WorldBudgetPIE). The final code did not receive a second full 158-case
run; the full result and subsequent focused correction are deliberately separate.

Two other rendering warnings remain recorded: the existing engine-side
`r.MotionVectorSimulation` render-thread warning (also present in the prior full
baseline), and a VSM non-Nanite marking-queue overflow in Fishing.CoastalLoopPIE.
The latter also appeared in earlier foliage validation and needs a rendering/
performance investigation in the connected world; no warning was suppressed,
engine file modified or shadow quality reduced to hide it.

Final Editor build `CoastalConnectionEditor09.log` and Development Game build
`CoastalConnectionGame02.log` succeeded, zero compile/link errors. Existing
toolchain preference and UE API deprecation warnings remain. Art validation
verified 143 generated files, 186 reviewed external files, 600 managed packages
and 381 locked Tidewater derivatives after the native map edits.
The later furniture intake raises the external-file check to 201 files; all
three sources remain evaluation-only and do not change the tested maps.

At native 1600×900, the reviewed voyage's sampled wall-frame median / p95 were
16.66 / 17.73 ms; GPU median / p95 10.50 / 12.09 ms. Sparse screenshot readbacks
remain in the run (maximum sampled wall frame 182.12 ms), so these are neither a
stutter-free claim nor a shipping performance certification. No render scaling
was reduced to obtain the image.

## Visual and acceptance limits

Native images are under `Saved/VisualOverhaul/<run>/UI/`:

- `PF007ConnectedShore02/PF-007-IslandFromShore.png`: island plainly visible in
  the normal player camera from the mainland pier.
- `PF006IslandBerth03/ReferenceBoat-Helm.png` and `ReferenceBoat-Chase.png`:
  actual connected-world boat viewpoints; water still too mirror-like.
- `PF009CoastalVoyage03/PF-009-IslandArrival.png`: real night arrival, fishing
  boat and lit pier visible; overall scene remains too dark for hero-view PASS.

The three key native captures are also retained, unmodified, under
[`Docs/Evidence/PlayerFeedbackCoast-20260927`](Evidence/PlayerFeedbackCoast-20260927/)
with source paths and SHA256 receipts. This keeps the limited visual evidence
reviewable after generated `Saved/` output is cleaned.

These are automation-driven native PIE images, **not manual playtest evidence**.
Open work: safe pier ladder/shore return, proper final mooring/boarding flow,
aerial distance/LOD review, waves and hull coupling, daylight forest readability,
night readability, designed interiors and the other feedback rows. The authored
pier's virtual ladder supports still need a native traversal adapter; no hidden
solid ladder wall or invisible teleport workaround was introduced.

Final manual PIE and user playtest remain required. No PF is promoted to final
PASS solely because the above tests or build succeeded. No Phase 21 work.

## Normal editor UI attempt

The regular UnrealEditor executable was launched with the ordinary South Beach
map and automatic persistence disabled to preserve existing player progress.
Computer-use discovered its actual window. Before a screenshot could be obtained,
capture returned `foreground window did not report a process id`; a fresh window
inventory then confirmed the editor was no longer open. The engine log records
PIE activity followed by `QUIT_EDITOR` and a clean exit, not a crash. No GUI image
or continuous input sequence was captured by this agent, and no user experience
conclusion is inferred from that log. Manual acceptance remains **unverified**.

After the attempt, both map hashes still matched the native authoring receipts.
The user's normal autosave and backup remained byte-identical to the pre-check
hashes (`01886422...F0D538C` and `06E27313...1AB03AC`). No normal save was deleted,
replaced or used as a test slot. `Edit-FileText.ps1` remains outside this change.
