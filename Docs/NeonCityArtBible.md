# NeonCity visual standards

Phase 20.5b direction: a readable contemporary coastal city. Warm limestone,
weathered off-white, restrained coral and petrol teal sit against dark asphalt,
cool glass and pale sand. Original designs only. This is a quality target, not
an assertion that every asset currently meets it. User visual approval is separate.

## Architecture and public space

Coral Court is residential: recessed windows, framed balconies, clear ground-floor
entrance and visible building services. Seabreeze Works uses deeper commercial
openings, roof services and a different frontage rhythm. Do not decorate flat
boxes with hundreds of identical windows. Corners, roof edges, storefront depth
and the pedestrian view matter as much as the front elevation. Background
buildings may be cheaper, but must share scale, materials and construction logic.

Roads use near-neutral asphalt, bounded repairs and legible markings. Drainage,
curbs, crossings and parking meters must make spatial sense. Furniture must not
obstruct established traffic routes, apartment doors or interaction approaches.
Damageable props retain their physical component and released destruction state.

## Vehicles

Judge silhouette and proportions before adding detail. Sedan, sports coupe, SUV
and pickup must be distinguishable without paint. Passenger volume, overhangs,
wheel arches and glazing need coherent transitions. Tires, paint, glazing,
rubber seals, machined metal and lamps have separate surface responses. No
stucco normal on car paint. No real manufacturer badges or copied signature forms.

Visual meshes conform to existing physics pivots, 32 cm road tire radius and
hardpoints. Do not alter a correct Chaos setup to hide a modeling error. Current
shared road chassis constrains roof height and wheelbase; a future approved
chassis project is needed for a genuinely longer-wheelbase pickup. Seat, dashboard
and steering-wheel shells are required; these are not finished usable interiors.

## Beach, water and marine

Read the sequence promenade → facilities → dry sand → wet sand → foam → sea.
Use low-frequency sand color variation, restrained fine normals and darker,
smoother wet sand. Foam must break up rather than form a bright uniform stripe.
The walkable slope must match its rendered surface. Preserve the established dock
and stair route. Finite prototype edges still require later spatial design.

Speedboat shape should show a useful bow, stern, coaming and helm. Yacht artwork
is an exterior/future-interior shell, not a new drivable class. Mooring hardware,
navigation lights and rails have plausible dimensions. Existing marine buoyancy,
water depth, deck support and ownership remain authoritative. Underwater color
and distant reflections need in-game checking; shader complexity is not quality.

## Aircraft

Helicopter cabin, tail boom, rotors, skids and engine housing must read as an
assembled airframe. Fixed-wing wings require an airfoil-like thickness, clear
cockpit and gear, coherent empennage and engine region. Weapons attach to existing
mounts; visual sockets must match those transforms. Searchlight direction remains
owned by the existing light component. No new flight model in an art pass.

## Weapons and characters

Pistol, SMG, rifle and shotgun need distinct length, magazine and stock profiles.
Grip, trigger, sight and muzzle must survive a player's normal camera distance.
Verify equipped hand position, aim, recoil and reload in continuous PIE frames.
A floating static gallery model does not validate attachment or animation.

Quinn skeleton and the original land animation pipeline remain compatible.
Palette variation is a foundation, not clothing geometry or a character creator.
Water motion uses deforming skeletal poses with shoulder, elbow, hip and knee
motion. Never use a static horizontal pose or body rotation to fake swimming.
Entry, exit and dive transitions need separate visual review from loop validity.

## Materials and lighting

Author at metres, export once to UE centimetres. No baked scene lighting in base
color. Use per-surface roughness; smooth glass/metal must not inherit masonry
detail. Prefer shared 512–1024 textures; 2048 only where visible density warrants
it. No blanket 4K. Translucency needs sorting/overdraw review. Keep night lights
readable without excess bloom; neon is an accent, not every surface. Review every
principal category in daylight, sunset and night with the normal game lighting.

## Starting asset budgets

These are authoring caps to review, not automatic quality certification. Actual
mesh counts/material slots are recorded in ArtAssetManifest and source ledgers.

| Category | LOD0 triangle target/cap | Slots cap | Texture target | Geometry strategy | Collision |
|---|---:|---:|---|---|---|
| Road vehicle body | 15–35k | 10 | 1–2K shared | 3 LODs; no Nanite glazing | existing Chaos body |
| Road wheel | 4–15k | 3 | 512–1K shared | 3 LODs, reuse four instances | original wheel/physics |
| Hero building | 60–180k | 9 | 512–1K tiled | Nanite opaque surfaces | retained shell |
| Background building | 8–30k | 5 | 512–1K | 3 LODs or measured Nanite | simple shell |
| Aircraft | 15–60k | 10 | 1–2K shared | separate rotor, 3 LODs | original airframe |
| Boat / yacht exterior | 15–70k | 10 | 1–2K shared | 3 LODs | boat hull; yacht none |
| Hand weapon | 4–18k | 6 | 1–2K | 3 LODs | existing weapon traces |
| Street prop | 0.5–12k | 5 | 512–1K | 3 LODs | retained body if breakable |
| Palm | 8–20k | 2 | 512–1K | 3 LODs; limit instances | none this pass |

New transport meshes currently use three reductions: 100/50/18 percent, screen
sizes 1/.22/.07. Buildings retain the previous Nanite strategy. Shared materials
avoid a full unique texture set per vehicle. Review LOD silhouette loss and actual
1600×900 Epic/100% runtime cost; report over-budget assets rather than hiding them.

## Acceptance for Phase 21

Do not expand the city by returning principal visible categories to greyboxes.
Each category needs authored source, provenance, scale/pivot/collision checks,
continuous evidence when animated, measured performance and user visual approval.
Mark a failed art gate **NOT COMPLETE**, even if import/build/tests pass.
# Phase 20.5c resource integration note

The current resource policy is licence-first discovery before new modeling.
See `ProfessionalAssetPipeline.md`, `ResourceDiscovery.md` and the asset manifest.
CC0 availability does not waive art direction: use the same centimetre scale,
coastal palette, material response and bounded texture/LOD budget as existing
NeonCity content. Asphalt uses restrained cool grey; the fine beach sand blends
from pale dry sand to a darker damp shoreline. Hydrant geometry is simplified,
grounded, resized and recoloured before use. Keep raw source files distinct from
the project derivatives. The downloaded gravel/algae sand is not used in UE.

The initial nine environment captures are a local checkpoint. The overall art
pass remains **ART PASS NOT COMPLETE**, including surrounding markings, pavement,
hero vehicles, character presentation and broader coastal detail.
