# Mountain jungle density correction

The user rejected the sparse mountain, including its distant silhouette, and
requested a jungle hiking experience. The source's separated tree crowns are
not an adequate acceptance target for this explicit density adaptation.

## Reversible authored layer

`DA_NCTW_Vegetation` and its 65,383 original records remain unchanged. The new
`DA_NCTW_MountainJungle` reuses the exact reviewed Tidewater tree, shrub, fern
and monstera meshes/materials in a separate tagged vegetation actor. No new
external asset or replacement primitive tree is introduced. The MIT notice and
source revision remain those in ArtAssetManifest.md.

| Family | Original island | Added mountain layer | Combined |
|---|---:|---:|---:|
| Trees | 12,745 | 20,510 | 33,255 |
| Shrubs | 5,598 | 6,254 | 11,852 |
| Ferns | 44,548 | 1,852 | 46,400 |
| Monsteras | 1,693 | 419 | 2,112 |

All other plant families are retained. Added trees mix overlapping mature crowns
with shorter subcanopy, and an irregular skyline; their horizontal scale is at
most 1.68. Ferns and broad leaves concentrate within 28 m of the source hiking
trail. Buildings, boardwalks, the pier corridor, water, scarps and exposed cliffs
retain exclusions. No landscape elevation or template movement setting changes.

The native terrain adapter places new roots against the actual 2,041-point R16
triangle heightfield, accounting for the source-Z row reversal. Trunk footprints
use the lowest ground sample so slopes do not expose floating bases. Independent
offline validation checks every one of 29,035 new records, identity uniqueness,
source hashes, finite transforms, grounding and path clearance. Minimum distance
from the hiking-path edge is 2.323 m for trees, 1.412 m for shrubs, 1.058 m for
ferns and 1.065 m for monsteras. Centre-ground embedding ranges 0.035–1.512 m.

A conservative crown-disk estimate sampled every 3 m rises from 25.39% to 88.35%
on eligible mountain forest ground. This excludes cliffs and is an offline
placement estimate, not a rendered-pixel or visual-quality measurement.

## Runtime and cost

The layer reuses the existing vegetation actor and seven near HISM groups plus
five distant groups. The 26,764 extra tree/shrub far representations are the same
plants, not additional population. Original 65/45 m near/far material transitions
and green hill palette apply. Distant cards do not cast dynamic shadows. There
are three shared dynamic materials for this actor; no per-tree actor, Tick,
runtime terrain trace or frame-by-frame scatter is added.

The five near tree/shrub meshes now use Nanite with Preserve Area, no interpolation
of semantic UV attributes, and the original fallback LODs. Materials explicitly
retain Nanite usage. A camera-to-player capsule fades obstructing foreground
foliage in the main view, while a Shadow Pass Switch preserves its shadows.
Distant view-facing cards are excluded from ray tracing and indirect-light
occlusion, so their undeformed cards cannot duplicate the nearby real geometry.
These native adaptations do not edit the original source buffers or shader ports.

The user's subsequent physical-contact request is implemented separately in
`NeonCityFoliageInteractionComponent`; see FoliageInteractionValidation.md. Hard
wood blocks actual movement and Chaos vehicles. Nearby soft plants bend and
recover through damped springs; no island-wide rigid-body simulation is used.
The single gameplay-camera LOD contract remains; split-screen is not certified.

## Verification status

Offline generation and the independent exclusion/grounding audit pass. Native
data import and placement are saved in the actual island level. JungleData03
passes both source and supplemental data tests. The earlier Data01/02 failures
exposed a test's incorrect grid-edge assumption: bounds describe cell origins,
while the final jittered cell may extend past the nominal edge. The corrected
check retains finite/upright/shader alignment and spatial-bound assertions.

JungleBuild05 and FoliageBuild04 / FoliageGame04 pass Editor and Game compilation.
JungleViews04 provides matched normal-player far, hill and forest fixtures;
the far silhouette is substantially denser. The near-view visual gate is still
open: Lumen surface-cache oversubscription appears on screen, the VSM non-Nanite
marking warning recurs on this denser route, and some sunlit gaps are excessively
bright. Neither hiding warnings nor disabling all forest shadows is a fix.

PhysicalJungle01 passes the approximately 133 m normal-input hiking leg after
wood collision is enabled. All eleven waypoints retain ground support and full
player health. The 107 actual native-ground probes embed roots by 3.147–116.888 cm.
At most 53 wood bodies are active across both layers; spatial lookups inspect at
most 654 candidate records, with zero capacity misses. PhysicalForest01 also
passes the original 80 m handoff route without disabling collision or changing
its targets. No template walk-speed or slope adjustment was needed. The source trail
above Z=-283 m has sections averaging more than 45 degrees; the planned walking
check ends before those sections and does not raise the player's walkable angle
or claim the entire summit route is traversable.

The final no-readback PhysicalJungleMetrics01 and full 145-case
PhysicalForestRegression01 are recorded in FoliageInteractionValidation.md.
Do not compare capture-frame timings against the older metrics-only forest leg:
the hiking route, density and screenshot readback differ.

Before comparison: `TidewaterForestShadowPolicy01/UI/IslandFar.png`, with the
same ordinary-player comparison fixtures at 11:00. All visual evidence comes
from the normal game viewport, not SceneCapture or a hidden replacement forest.
This correction does not close the other Phase20.5c direct-port gates.
