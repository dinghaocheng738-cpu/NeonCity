# Canonical island village — integration evidence

Phase 20.5c, **TIDEWATER PARITY TARGET; incomplete integration**.
Reference: `dgreenheck/tidewater@4811ba48d795197de5621985f404e765c0b7c0ef`.
Author: DRG Software Solutions LLC (2026), MIT; full notice retained and staged.

## Source and conversion boundary

The reviewed CPU constructors use the same terrain seed 7 and village seed 90210
as the native island. Export asserts exact agreement with the imported building
pads. Source metres `(X,Y,Z)` become UE centimetres `(X,-Z,Y)*100`; winding,
normals, five UV channels, linear vertex colours and moving-part pivots are
converted together. Source arrays remain immutable and hash checked.

The export contains 31 surface pieces / 141,396 triangles, 17 houses and four
sheds, plus the source market, boathouse, boardwalk, pier and fittings. The 78
fish/ice/leaf/lobster instances are exported separately and are **not yet native
world content**. Scan-prop placement and named vendor interactions remain open.

`Village/Source.json` records the exact modules, raw geometry, materials and
texture-generator hashes. Blender sources and 31 FBX derivatives retain the
canonical geometry. A separate invisible cylinder with one convex hull represents
analytic pile contacts; it is collision infrastructure, not replacement artwork.

## Materials

Nine original procedural field generators have bounded WGSL-to-HLSL adaptations;
all pass standalone DXC. Seven surface programs and two cloth/net vertex programs
also pass standalone DXC (`TidewaterVillageMaterialShaders04`). Seven native UE
materials now compile, including ray-hit permutations. Texture sampling uses the
engine wrapper's ray-cone LOD rather than illegal implicit-derivative ray sampling.

The native bake pipeline draws linear colour/alpha to float32 targets and height,
roughness and AO fields to float16 targets, matching source intermediate storage.
The second pass uses the source Sobel slopes and eight-direction, two-radius
horizon-AO equations. `TidewaterVillageBake02` and the deterministic texture pass
produce fourteen linear RGBA maps. Native intake validates all 31 five-UV meshes,
three LODs, seven materials and the separate convex contact asset (53 packages).
The material instruction/sampler counts are recorded in `Village/Import.json`;
the most expensive opaque wood material reports 768 pixel instructions and five
samplers. UE lighting replaces the browser renderer; photometric intensity,
night exposure, translucency and combined performance still need calibration.

## Runtime and collision

`ANeonCityReferenceVillage` owns exact source 394 oriented-box and 168 cylindrical
contacts. Of 155 source walkable boxes, 147 are real solid floors and eight are
non-solid virtual ladder supports. These ladder volumes are kept nonblocking;
turning them into physical boxes would create a solid wall. Native water-climb
adaptation remains explicit unfinished work. Static visual geometry has no
separate collision, avoiding duplicate enclosing hulls. Placement02 checks all
147 solid floor hits and all eight nonblocking supports before saving the map.

Source sign/lantern spring equations use bounded integration steps; native cloth
and net materials retain source wind displacement. Fifty authored light positions
now register with WorldLightingSubsystem: one 4 Hz scheduler caps the combined
world at 24 active local lights and six shadows, with distance tiers and persistent
far emission. This supersedes the initial village-only eight-light/shadowless
budget. Distant pendulums still reduce their update rate. Combined performance
has not been measured and no performance acceptance is claimed.

## Current evidence boundary

`TidewaterVillageEditor06` compiles and links successfully. Native intake is
recorded in `Authoring-20260926-135525`; placement in
`Authoring-20260926-135719`. All four daylight images from
`TidewaterVillageNativeReview01` were inspected: the real pier, curved boardwalk,
market canopy, painted cottages and material detail are visible. No ocean,
vegetation, scan dressing or vendor characters are present in these images.

Two night images in that same first session are invalid visual evidence: the
abrupt daylight-to-night change retained bright capture history. Separate-session
`TidewaterVillageNativeNight01` removes that artifact but reveals insufficient
lighting. Local lights previously used UE's unitless default; they now explicitly
use lumens. `TidewaterVillageNativeNight02` shows actual illumination on the pier,
but the overall island and route are still too dark: **NIGHT PLAYABILITY FAIL**.
These diagnostic captures and failed images are kept; they are not normal-game
night acceptance or a substitute for the requested before/after route.

`TidewaterVillageWalk01` passes the ordinary-player PIE route from the pier head,
along the pier, up the shore steps and along the source boardwalk. After its one
initial test fixture placement it uses normal movement input and checks native
Walking state, support height and unchanged health. `TidewaterVillageGame01`
compiles and links successfully. `TidewaterBoatVillageRegression02` passes all
134 registered cases, zero errors, in 1573.90 s. Its two engine warnings concern
motion-vector console access and VSM marking-queue capacity; foliage/ocean combined
performance remains unaccepted. The
island remains a separate sublevel awaiting the continuous mainland-to-island
marine route. No source NPC/UI branding is adopted; legal attribution is intact.

Subsequent normal-game night comparisons and the late clock/exposure corrections
are recorded in `WorldLightingValidation.md`. Historical failed diagnostic images
above remain preserved. A readable fixed pier view does not replace the requested
continuous night approach, landing and village route.
