# TIDEWATER PARITY TARGET — Phase 20.5c

Canonical reference: `dgreenheck/tidewater@4811ba48d795197de5621985f404e765c0b7c0ef`.
**Overall: NOT COMPLETE. No row below is accepted parity yet.**
**Critical correction gates remain open: VEGETATION PARITY and NIGHT PLAYABILITY
are not accepted.** Native canopy and lighting progress below supersedes the
initial bare-hill/dark-pier state, but not continuous acceptance. See the direct-port/night
requirement and integration document. Normal-game comparison is mandatory.
Source paths below are relative to the pinned checkout. A source pointer is an
inventory finding, not a claim that its complete implementation has been audited.
Detailed port review and license clearance occur before each adoption. See
`ThirdParty/TidewaterIntegration.md` for the audit boundary and source notices.

Status: **R** remaining; **P** existing NeonCity prototype/partial (must upgrade and
compare); **A** accepted only with linked numerical/PIE/visual evidence. Every row
requires normal-player integration and performance acceptance, where relevant.

## Fishing

| Feature | Source | State | Required acceptance / remaining difference |
|---|---|---|---|
| R equip/stow on shore and deck | game/Game.js | P | Existing F station start must become contextual R with combat separation |
| Spinning rod at real scale | game/FishingRod.js | P | Compare proportions and in-hand framing against source; existing CC0 rod retained pending review |
| Animated reel, bail, rotor, crank | game/FishingRod.js | R | Visible winding, casting and reeling, not static detail |
| Spring rod bending | game/FishingRod.js | R | Load, flick and recovery track line force |
| Hold LMB windup / release cast | game/FishingRod.js, Game.js | R | Charge changes actual range; no instant fish award |
| Ballistic bobber, line sag, water landing | game/FishingRod.js | P | Existing interpolated cast/line must follow physical water queries |
| RMB empty-line retrieval | game/Game.js | R | Abort flight or retrieve floating line; restore casting readiness |
| Nibble then take cues | game/Game.js | R | One to three nibbles; early strike hint; species-dependent take window |
| Strike / missed take recovery | game/Game.js | P | Match source retry semantics and visual/audio cues |
| Surge / stamina / line-length fight | game/CatchMinigame.js | P | Faithful native model integrated, 112 source oracle cases pass; mature cast/rod/deck presentation still required |
| Green tension band / overload | game/CatchMinigame.js | P | Accumulated overload, slack loss and spool-out, not immediate threshold only |
| Line snap / escaped fish | game/CatchMinigame.js | P | Observable cause and feedback, no catch inventory on failure |
| Catch card and fish portrait | game/GameHUD.js, FishPortrait.js, CatchDisplay.js | P | Readable length/weight, species, keep/release and presentation parity |
| Length-weight calculation | game/FishTable.js | P | Source power relation; existing independent random length is insufficient |
| Eighteen catchable species | game/FishTable.js, world/fish/FishSpecies.js | R | Data, legal geometry/materials and habitat differentiation |
| Personal records / new species | game/GameState.js | R | Records survive sale, release and restart |
| Fish log | game/GameState.js, GameHUD.js | R | Count, best weight/length and species browsing |
| Cooler / fish hold capacity | game/GameState.js, Gear.js | P | Weight capacity, log oversize catch even when released |
| Shallower, pier, reef, bay, deep habitats | game/Bites.js, Game.js | P | Overlapping depth/location weights, not one Coastal tag |
| Depth-dependent bite rates | game/Bites.js | R | Dry land cannot bite; rich water changes waiting time |
| Dawn/dusk/day/night activity | game/Bites.js | P | Continuous source activity curves and source species preferences |
| Skewed trophy size distribution | game/Bites.js | R | Match source weight roll and plausible maximum sizes |
| Line upgrades | game/Gear.js, Chandlery.js | R | Rating materially changes fight outcomes and saves |
| Reel upgrades | game/Gear.js, Chandlery.js | R | Actual line retrieval speed and saved level |
| Rod upgrades | game/Gear.js, Chandlery.js | R | Actual cast range and saved level |
| Fish-hold upgrades | game/Gear.js, Chandlery.js | R | Capacity, cost and safe inventory recovery |
| Fish finder | game/Game.js, Gear.js | R | Local depth/richness at bounded query rate |
| Night fishing | game/Game.js, Gear.js | R | Activity, floodlights and readable cues at night |
| Fish sale and upgrade trade | game/FishStand.js, Chandlery.js | P | Existing sale works; native economy adapters and renamed vendors required |
| Weather extension | NeonCity requirement; source sea settings | R | Explicit NeonCity extension; do not invent upstream behavior |

## Boat and player interaction

| Feature | Source | State | Required acceptance / remaining difference |
|---|---|---|---|
| High-fidelity fishing boat geometry | world/BoatModel.js, world/boat/* | P | Source 46,845 triangles and seven native surface programs; documented 40 cm chair translation for native seated reach; inspected native/gameplay views; final combined acceptance pending |
| Boarding from pier, shore and water | player/Player.js | P | Safe collision-resolved access, feedback and return |
| Take helm / leave helm | player/Player.js | P | Return to walkable deck, preserve inertia |
| Walk cockpit/deck/wheelhouse | player/Player.js, world/boat/Wheelhouse.js | P | Native CharacterMovement based movement; door and rail clearance |
| Drift while unpossessed | player/BoatController.js | P | Source force port passes 14 oracle cases; ordinary-input PIE drives, leaves helm, stands and catches on coasting deck; final full route pending |
| Fuel / empty tank | game/Game.js, Gear.js | R | Burn at helm, engine ceases thrust safely, ownership/save integration |
| Refuel service | game/Chandlery.js, GameState.js | R | Partial affordable refill, correct payment and state |
| Engine upgrade | game/Gear.js, Game.js | R | Thrust/top speed change without replacing physics |
| Fuel tank upgrade | game/Gear.js | R | Capacity and persisted ownership |
| V first/third person toggle | player/Player.js | P | Shared Speedboat/Fishing Boat/Yacht framework |
| Interior first-person helm view | player/Player.js, world/boat/Wheelhouse.js | P | Visible helm, dashboard, windshield and hull, no outside camera |
| Smooth camera / partial stabilization | player/Player.js | P | Roll/pitch comfort while preserving boat reference |
| Helm controls and instruments | world/BoatModel.js, boat/Running.js | P | Steering/throttle animation and readable HUD |
| Deck fishing anywhere safe | player/Player.js, game/Game.js | R | Cast/fight/catch on drifting deck without fixed yellow marker |
| Deck floodlights | game/Game.js | R | Night water/deck illumination with bounded light cost |
| Mooring / safe step ashore | player/BoatController.js, Player.js | P | No unexpected freeze away from dock; safe moving exits |
| Hull buoyancy, resistance, rudder and grounding | player/BoatController.js | P | Preserve Chaos; compare response, spray/wake and contact behavior |

## Ocean and coast

| Feature | Source | State | Required acceptance / remaining difference |
|---|---|---|---|
| Four-cascade FFT spectrum | ocean/OceanFFT.js | P | Original WGSL and native UE GPU fields pass independent NumPy and 243-field cross-backend comparisons; bounded queries pass 704-point checks and full 152-case regression; material and actual scene integration pending (OceanFFTValidation.md, WaterQueryValidation.md) |
| Swell and wind-wave scales | ocean/OceanFFT.js, WaterSurface.js | R | World-space coherent waves from near shore to horizon |
| Whitecaps and foam | ocean/FoamTexture.js, WaterMaterial.js | P | Native FFT foam, original 11-mip texture and 28,128 same-input source/native vertex evaluations validated; exact CDLOD selections/mesh pass; visible surface, coast and distance behavior still pending (OceanSurfaceValidation.md) |
| Wind streaks / small sea detail | ocean/SeaDetail.js | R | Stable shading and alias control |
| Depth-aware shore waves | ocean/ShoreWaves.js | R | Bathymetry drives shape and shoaling |
| Breaking waves / peeling shoulders | ocean/Breakers.js | R | Temporal reference comparison from coast and boat |
| Whitewater / breaking spray | ocean/Breakers.js, fx/Spray.js | R | Budgeted equivalent; near-camera emitters and distance budgets |
| Foam lace | ocean/SurfFoam.js | R | Preserve fine coast texture and motion |
| Swash / run-up / retreat / backwash | ocean/ShoreSim.js | R | Shallow simulation follows real island profile |
| Wet sand / receding shoreline | ocean/ShoreSim.js, world/terrain/TerrainShading.js | R | No hard moving ruled edge; wetness persists plausibly |
| Boat wake | ocean/WakeSim.js | R | Speed/direction/turn response, including leaving helm |
| Bow spray / slam spray | player/BoatSpray.js | R | Water-relative impact and speed response |
| Whale wake / blow / splash | ocean/WhaleWater.js, world/marine/Whale.js | R | Integrates shared water effects |
| Seabed refraction behind objects | ocean/RefractionPass.js | R | Pier/boat depth occlusion correct |
| Shared displaced water queries | ocean/WaterQuery.js | P | Boats, swimmer, bobber and visible surface agree |

## Swimming and underwater

| Feature | Source | State | Required acceptance / remaining difference |
|---|---|---|---|
| Surface swimming / wave following | player/Player.js | P | Native movement extension and water-query agreement |
| Dive / ascend / underwater swim | player/Player.js | P | Controls, camera and collision through reef route |
| Reef exploration | world/Reef.js | R | Navigable coral, sand/rock and depth variation, not distant scenery |
| Caustics on seabed | ocean/Caustics.js | R | Animated and depth/sun aware |
| Underwater light shafts | ocean/UnderwaterLighting.js | R | Physical atmosphere/light integration |
| Underwater haze / visibility | post/Underwater.js | R | Depth/path-length absorption with stable transitions |
| Surface seen from below | ocean/WaterMaterial.js, post/Underwater.js | R | Correct orientation/reflection/refraction |
| Split waterline view | post/Underwater.js | R | Continuous above/below composition |
| Surfacing lens droplets | post/LensDroplets.js | R | Trigger, motion, decay and no permanent obstruction |
| Underwater flashlight | player/Player.js, materials/LocalLights.js | R | Beam, haze and local surfaces visibly respond |
| Marine snow | fx/MarineSnow.js | R | Bounded near-camera particles |

## World and wildlife

| Feature | Source | State | Required acceptance / remaining difference |
|---|---|---|---|
| Exact island footprint | world/TerrainData.js, WorldLayout.js | P | Native 64-component Landscape, source seed 7 and nine collision probes; mainland streaming/playthrough pending |
| Hills / ridge / gullies / volcanic plugs | world/terrain/IslandShape.js, TerrainNoise.js | P | Exact source heights imported; native terrain-only renders inspected, dressed-world comparison pending |
| Beach placement / dunes / scarp | world/TerrainData.js | P | Source field/masks and native shader imported; swash/ocean/ground-level walk still required |
| Headlands / sea cliffs / sea stacks | world/TerrainData.js, terrain/IslandShape.js | P | Source silhouette retained; independent rock geometry and final lighting/shore comparison pending |
| Shoreline / shallows / deep offshore | world/TerrainData.js | R | Bathymetry and shore simulation use same field |
| Beach-to-reef transition | world/TerrainData.js, Reef.js | R | Exact reef center and local depth, seagrass/rubble |
| Rock formations | world/Rocks.js, terrain/RockGeometry.js | R | Legal geometry export and LOD |
| Pier relationship | world/Pier.js, WorldLayout.js | P | Canonical T head, deck height, fittings and source floor contacts in native island; eight virtual ladder supports need native climb adapter; water/berth route remains open |
| Fishing village spatial layout | world/Village.js, village/* | P | Canonical 31 pieces / 141,396 triangles and seven surface programs placed; 147 solid support probes pass and four daylight views inspected; vendors, scan dressing, fish displays, night and player route still under validation |
| Vendor stalls / scanned props | game/StallKit.js, public/models/props | P | 22 individually cleared source assets in native library; placement and can label removal still required |
| Animated vendor characters | game/Vendor.js, public/models/characters | P | Rocketbox MIT verified; two native skins/14 original clips/three LODs; native pose review and NeonCity vendor integration |
| Coral structures | world/Reef.js, reef/* | R | Source morphology/materials, collision and exploration scale |
| Reef fish | world/Fish.js, fish/* | R | Schooling, species geometry, depth/region placement and LOD |
| Tropical vegetation zones | world/Vegetation.js, vegetation/* | P | All ten source families / 65,383 instances in native HISM; matched game views inspected, continuous route open |
| Trees / shrubs / dune grass | world/vegetation/* | P | Exact canopy/wind and 180-view handoff; source grass and bounded tree/palm/shrub contact integrated, full 151-case regression and contact output pass; daylight balance and combined performance remain open |
| Beach debris / scanned driftwood / shells | world/Debris.js, debris/* | P | Four same-source CC0 scans imported with original LODs; shoreline distribution pending |
| Birds / gulls / shorebirds | world/Gulls.js, wildlife/* | R | Flight, ground behavior, calls, pooling and LOD |
| Crabs | world/wildlife/Crabs.js | R | Shore/swash behavior and near-player reactions |
| Whale swimming / escort fish | world/marine/Whale.js, WhaleBrain.js | R | Stateful visible animal with distance simulation |
| Whale blow / dive / breach | world/marine/Whale.js, WhaleBrain.js | R | All representative behavior retained or explicit technical gap |
| Mainland-to-island relationship | NeonCity integration | R | Offshore exception only; no main city replacement or Phase21 areas |

## Sky, lighting and post

| Feature | Source | State | Required acceptance / remaining difference |
|---|---|---|---|
| Physical atmosphere / aerial perspective | sky/Atmosphere.js | P | Compare UE SkyAtmosphere at matched sun/view/time |
| Sun / moon / stars | sky/Sky.js | P | Day/night continuity and licensed star resources |
| Cumulus / cirrus | sky/Clouds.js | R | UE volumetric equivalent with comparable structure |
| Cloud shadows | sky/Clouds.js | R | Motion and land/water coverage |
| Sea haze | post/AirHaze.js | R | Coast/offshore depth readability |
| God rays | post/AirHaze.js | R | Occlusion and atmosphere interaction |
| Occluded lens flare | post/LensFlare.js | R | No flare through opaque world |
| Lanterns / windows / boat night lights | materials/LocalLights.js | P | Island/source presentation and GPU budget |
| Soft/contact shadows / bounce / AO | engine/render/Shadows.js, materials/*, post/GTAO.js | P | Native UE equivalents; compare rather than copy renderer |
| Bloom / exposure / motion blur | post/PostFX.js, MotionBlur.js | P | Stable transitions with no excessive post processing |
| Temporal upscale / sharpening | post/TemporalUpscale.js | P | UE equivalent under target laptop combined workload |

## Audio and UX

| Feature | Source | State | Required acceptance / remaining difference |
|---|---|---|---|
| Surf timed to waves | audio/SoundScape.js, public/audio/CREDITS.md | R | Verify source license; spatial phase-aligned breakers/wash/backwash |
| Wind / palms / insects / dawn birds | audio/SoundScape.js | P | Locked source library/mixer, bounded native voices and non-silent game recording verified; subjective/full-route acceptance open |
| Boat engine / hull slap / rush / lap | audio/SoundScape.js | R | RPM/load/contact, inside/outside and drift |
| Surface-dependent footsteps | audio/SoundScape.js | P | Grounded travel with moving-base removal and grass/sand/wet-sand/known wood verified; full water/rock routing and animation timing open |
| Swim / submerge / emerge / underwater | audio/SoundScape.js | R | Transition, filtering, occlusion and loops |
| Whale song / blow / breach splash | audio/SoundScape.js | R | License all mix constituents; distance timing |
| Rod / reel / drag / line / bail / snap | audio/SoundScape.js, soundBank.js | P | Existing two clips are partial; source set and state synchronization |
| Plop / fish splash / flop / sale | audio/SoundScape.js | P | Spatial/state cues; no unknown placeholder |
| Contextual guide / first-play / replay | game/Guide.js | P | Native guidance for full boat/fishing/diving loop |
| Minimap / landmarks / guide highlights | game/Minimap.js | P | Island shape and world markers from actual layout |
| Controls help | ui/UI.js | P | Accurate active-context bindings and existing phone compatibility |
| Photo mode | ui/UI.js | R | Pause input conflicts; native equivalent and useful capture |
| Settings / time / sea / audio controls | ui/UI.js | P | Native equivalents and explicit mapping; debugging panels not mandatory copies |
| Save / progress / records / upgrades | game/GameState.js | P | UE SaveSubsystem snapshots, stable IDs, migration and restart tests |
| Error / loading / recovery feedback | ui/UI.js | P | Safe asset/load failures, no browser-only implementation copied |

## Cross-system acceptance gates

- [ ] Full existing regression remains green after each integrated milestone.
- [ ] Differential numerical tests use the exact locked source, including failure cases.
- [ ] Every adopted third-party asset has a completed manifest record and preserved notice.
- [ ] Source-vs-UE views compare matching location/time/scale and temporal effects.
- [ ] One continuous player recording: South Beach → boat 3P → V 1P → leave helm →
  walk deck → cast/fight/catch → helm → island → coast/swash → dock → island/pier
  fishing → reef swim/dive/flashlight/caustics → surface droplets → boat → return.
- [ ] Combined ocean + reef + wildlife + boat + fishing frame time, memory and
  streaming measured on RTX 5060 Laptop; no competing heavy tool jobs.
- [ ] Human perception/feel acceptance is distinguished from automated observations.
- [ ] Remaining differences state exact license/engine/performance/technical cause.
- [ ] After parity: finish aquarium, cooking, fish pen, yacht, submarine, torpedo,
  marine economy/weather/missions/ownership and original 20.5c workstreams.
- [ ] Stop before Phase 21 and wait for the user's final playtest.

Boat evidence: `MarineReferenceBoatValidation.md`. Fixed deck station is transitional; it does not satisfy the any-safe-deck R fishing requirement. No row is promoted to A by isolated tests.

## Mandatory vegetation and night corrections

- [ ] Exact source palms, broadleaf canopy, shrubs, bananas, monstera, elephant
  ear, heliconia, bird of paradise and dune/meadow/sea-oat/creeper distribution.
- [ ] Human-use clearances and differentiated beach/lower/mid/upper/headland zones.
- [ ] Distant canopy survives boat approach and helicopter view with bounded LOD.
- [ ] One central lighting manager with nearby, middle and emissive-only tiers.
- [ ] Automatic dusk/dawn road, sidewalk, junction, parking, entrance, storefront,
  varied window and marina lighting actually illuminates traversable surfaces.
- [ ] Island lantern, pier, village, path and dock lighting preserves island style.
- [ ] Cars' head/tail/brake and emergency lights, marine navigation/deck lights,
  aircraft navigation and helicopter searchlight work in the night experience.
- [ ] Contextual player/diving flashlight does not conflict with helicopter L.
- [ ] Restrained moon/ambient/fog and exposure keep night dark but playable.
- [ ] Continuous normal-game night routes A–F and matched eight before/after views.

Canonical vegetation intake exports all ten source families (12,745 canopy trees,
5,598 shrubs, 233 adult palms and source understory), without random resampling.
The native map now retains all 65,383 source placements and differentiated zones,
including the source village clearances. Three matched normal-game views show
upright hillside forest. The requested greener hill treatment is inspected in
TidewaterForestColour04 and the same grade in native far atlases. The 180-view
handoff, source shadow policy and 80 m normal-movement route are checked in
TidewaterForestShadowPolicy01 / ShadowMetrics01. Native source grass and bounded solid tree/palm
plus flexible understory/contact audio now pass full 151-case regression. Daylight balance,
combined performance and the continuous boat/climb/flyover route remain open;
no vegetation parity pass is claimed.
