# Phase 20.5c — island bird rebuild

Scope: replace rejected placeholder birds. Phase 21 is not started. This record is not acceptance of the entire Polish Pass.

## Art and provenance

- Seagull v1 by Julian Johnson-Mortimer / FreddyFoxFreddy: CC BY 4.0. Source page, public download mirror, original API license snapshot and file hashes are retained in `ArtSource/Polish/ReviewedBirds`.
- Yellow-Billed Shrike by pistachio: CC0 1.0, OpenGameArt. Original archive retained alongside the attribution record.
- See `ArtSource/Polish/ReviewedBirds/LICENSES.md` for exact links, attribution and modifications. The previous Poly low-poly gull is excluded from native presentation.
- Blender source files and reproducible adaptation/import scripts retain source UVs and feather textures. Work includes scale normalization, redundant gull foot geometry reduction, shrike silhouette smoothing, repaired PBR connections, a flight rig and a folded-wing morph.
- Wingspans: gull approximately 1.33 m; small bird approximately 0.34 m. Nonmetallic feather materials, masked feather edges, opaque gull body and shrike normal map. No primitive mesh substitutes or unassigned material slots.

## Runtime

`AIslandBird` uses actual skeletal wing animation and a folded-wing morph. Its basic loop includes supported outdoor rest, takeoff, turning circular flight, approach and landing. Flight phases and rest periods vary across the 16 authored birds. Nearby players disturb a resting bird. World collision sweeps reject obstructed flight steps; support, overhead and water checks reject invalid perches before showing a bird.

The map contains 10 gulls around the pier, coastal rocks and villa roof, plus 6 small birds on village/hillside rock perches. These are persistent authored habitats, not camera-relative spawns. No bird billboard substitution or distance mesh swap is configured. This deliberately small population retains the same skeletal representation at distance; it is not a large-scale flock simulation.

## Verification

Real main-map PIE tests exercise both species' full flight cycles, material assignment, permanent habitats, water exclusion, continuity, near/far views, night views and occupied helicopter flight. Fixtures and automated inputs are recorded explicitly; these tests do not imply a manual city-to-island trip.

Earlier rejected runs are retained: off-pier test fixtures, missing saved material slots, small-bird landing clearance, upside-down authored villa roof and prematurely captured night exposure. Each was investigated separately. The villa/reef authoring rotation defect was repaired with explicit yaw; the associated repair receipt records map hashes and changed actors.

`PolishIslandReview08` passed all four checks: BirdCandidatePIE, BirdHabitatPIE, IslandFlyoverPIE and VillaPIE. The report contains one engine warning about `r.MotionVectorSimulation` render-thread use. All 16 authored birds were visible and completed outdoor flight cycles. The occupied, physically simulated helicopter followed a circuit exceeding 500 m after one explicit initial fixture placement; 26 sequential screenshots were retained in Saved. A final bird-only run follows the small-bird folded silhouette adjustment.

Visual review covered textured gull/shrike close-ups, wing movement, daylight, a lit bird at night, unchanged skeletal representation at 65 m and 155 m, and adjacent helicopter flyover frames. Night exposure needs time to adapt after a test jumps from daylight to 21:00; early captures were rejected. This is not an all-island lighting acceptance or a full performance benchmark.

Final results: `PolishBirdFinal09` passed both bird tests after the compact shrike fold adjustment (16 visible / 16 completed cycles). `PolishBirdDistance10` passed with exposure settling in both directions; its 65 m and 155 m captures replace the overexposed distance captures from earlier runs. Raw screenshots, reports, logs and hashes are retained under `Docs/Evidence/PolishBirds-20260927`. Native asset and controller hashes are in `ArtSource/Polish/ReviewedBirds/native-assets.json`.

The canopy shader previously stopped interpolating view frames beyond 100 m and selected a single dominant frame. It now retains barycentric view interpolation throughout the visible range, removing that hard distance/angle switch. Source shader compilation and UE material compilation passed. Near/far geometry handoff and culling ranges are retained. Distant pixels sample more atlas frames; measured full-scene performance budgeting remains a separate task.

## Commit boundary

The focused bird/library commit contains licensed originals and adaptations, native bird assets, flight controller, reproducible preparation/import tools, canopy fix and retained evidence. The current main map contains earlier uncommitted immersion work together with this turn's reef/diving/villa/habitat placement. That binary map and its broader dependencies remain in the working tree; this commit is not a clean-checkout delivery of the complete island or all Phase 20.5c features. The habitat receipt records the tested working-map placement. The integration test sources likewise remain with that broader working-tree integration.

Original Phase 20.5c mountain road, wingsuit, off-road vehicles, full diving-suit presentation and other outstanding acceptance items remain incomplete. Do not mark the entire Polish Pass complete or enter Phase 21.
