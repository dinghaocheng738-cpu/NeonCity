# Canonical ground flora — integration in progress, 2026-09-26

This is a direct native adaptation of the reviewed Tidewater GrassField.js at
4811ba48d795197de5621985f404e765c0b7c0ef (MIT). Original geometry, density masks,
species choices, tier fades, wind and colour equations are retained. It is not
a replacement set of randomly placed grass cards.

## Source and native representation

The original 2,724 occupied 8 m cells select three shared patch meshes. Each cell
has 384 grass clump slots, plus the source sea-oat and creeper slots. Near/mid/far
patches contain 17,560 / 4,716 / 768 triangles; eight full-precision UV channels
carry original slot, blade, side and source-position attributes. Shared blades
retain identical roots and tips between levels. The source budgets remain
48 / 160 / 420 cells, with 18 / 46 / 88 m ranges and original blade-width fades.

Only the canonical vegetation owner supplies DA_NCTW_Grass. Supplemental jungle
actors do not duplicate it. The existing observer/wind update drives one component
with three transient instanced meshes. There is no per-blade Actor, Tick, collision
body or ground trace. Cell occupancy is indexed once, candidates use the local
view region and frustum, and instance lists update only when selection changes.
The original grass receives lighting but casts no shadow. Undeformed preview
triangles do not enter hardware ray tracing or consume Lumen cards. The shared
world lighting scheduler supplies the existing night parameter.

## Grounding

The source's 2,048-sample field differs from UE's 2,041-sample triangulated
Landscape by as much as 54.25 cm at visible source slots. Reusing that source
bilinear height directly would float or bury roots. The native adapter uses the
actual landscape's heights, diagonal, origin and spacing. An RGBA8 data texture
stores exact float32 bytes; it is linear, uncompressed, unmipped and nearest
sampled. The shader reconstructs native triangle height for each source slot.

TidewaterGrassSource02 checks 1,046,016 root slots, including 654,530 nonempty
mask slots. The float32 native triangle adapter has maximum error 0.000283 m
and p99 0.0000603 m against its double-precision calculation. This is offline
numeric evidence, separately from actual gameplay and image inspection.

## Evidence so far

- TidewaterGrassBuild01 Development Editor Win64 succeeds in 33.00 s.
- Native import Authoring-20260926-193223 succeeds, followed by saved placement
  Authoring-20260926-193330. Source geometry, Blender, FBX, shader and package
  hashes are retained. The material compiles at 314 pixel / 729 vertex shader
  instructions and two reported samplers on this native configuration.
- TidewaterGrassData01 passes native topology, all UV channels, source cell
  identity and height texture checks.
- TidewaterGrassWalk01 passes ordinary third-person coastal walking without
  changing movement. Native plants are visible, health stays 100 and all patch
  budgets hold. Dune view: 16/36/90 active cells; oats/creeper view: 15/21/36;
  meadow view: 17/36/8. The normal viewport and continuous frames are retained.
- Dune and meadow stills were inspected. Roots appear on terrain and the native
  source player is retained, but grass is too pale/bright in the current lighting.
  A VSM non-Nanite marking warning remains. These images are **not** a final
  colour, lighting, performance or complete-island acceptance.

## Open gates

Grass currently has original wind but no per-pawn contact deformation. Existing
nearby shrubs/ferns/monsteras use the separate physical spring component; do not
claim that this grass already responds to the player's body. Native lighting
balance, normal-game visual review, performance and the expanded 147-case full
regression were required for this checkpoint. PalmContactBuild02 compiles Editor
and Game successfully. TidewaterGrassFoliageRegression01 now passes 147/147 tests
in 1731.05 s, including this grass and the expanded source-shaped palm contact.
Two existing engine warnings remain; no test or reflected-class startup errors.

Mainland/island continuity and all remaining Phase20.5c workstreams remain open.

TidewaterGrassPalmWalk02 repeats the actual coastal/meadow route after the
vegetation-mask repair, palm collision and rendering-budget changes. It passes
with two recorded engine warnings (motion-vector console access and VSM marking).
The final meadow frame was inspected: grass is grounded and dense, but remains
too pale under daylight. Colour/lighting acceptance is deliberately still open.
