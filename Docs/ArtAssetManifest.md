# NeonCity art asset manifest — Phase 20.5 through 20.5c

The initial Phase 20.5 entries below were authored locally, before the later
resource-discovery gate. Phase 20.5c adopted external assets are recorded in the
dedicated sections below with their authors, licences, sources and derivatives.
No private project content was uploaded. The initial buildings, props, textures
and animation curves are original work in this repository. The character mesh/rig is the project's existing
Epic Third Person Template content; its Epic/Unreal Engine licence still applies.
Do not relicense that reference as original NeonCity artwork or redistribute it
as a standalone asset. It is separate from the CC0 assets listed later.

## Authored static assets

Editable source: `ArtSource/Buildings/SouthBeach/SouthBeachReferenceBlock.blend`.
FBX files beside it use the names below. UE destination prefix is
`/Game/NeonCity/Art/SouthBeach/`; directory is Buildings, Props or Streets.
Numbers are imported source geometry before Nanite/LOD reduction.

| Name after SM_SB205_ | Directory | Dimensions, metres | Triangles | Slots |
|---|---|---|---:|---:|
| CoralCourt | Buildings | 17.80 × 18.40 × 16.65 | 177252 | 9 |
| SeabreezeWorks | Buildings | 18 × 22.50 × 15.2165 | 68504 | 7 |
| EntryPortal | Buildings | 3.20 × 1.95 × 3.41 | 14424 | 3 |
| FacadeBay | Buildings | 3.20 × 1.65 × 3.20 | 5708 | 5 |
| Bench | Props | 2.05 × .660 × .933 | 4528 | 3 |
| LitterBin | Props | .70 × .70 × .95 | 6412 | 3 |
| StreetLamp | Props | .42 × 1.61 × 5.55 | 3032 | 4 |
| Planter | Props | 1.659 × .928 × 1.115 | 5116 | 3 |
| StreetSign | Props | 1.35 × .105 × 2.90 | 13736 | 3 |
| Road12m | Streets | 12 × 12 × .025 | 12 | 1 |
| Paving4m | Streets | 3.994 × 2.994 × .030 | 9024 | 1 |
| Probe | Probe | 4 × 2.245 × 3 | 376 | 2 |

Probe source: `PipelineProbe.blend`. Heroes use Nanite. Other detailed assets
have three LODs (100%, 50%, 20%; screen sizes 1, .18, .05). Road is a minimal
surface skin over retained physical road collision. Artwork does not create
duplicate physics surfaces. Building foundations extend .40 m below the reference
ground origin to close the old terrain/plaza height gap.

## Materials and textures

`ArtSource/Textures/SouthBeach/T_SB_<material>_{BC,R,N}.png` contains twelve
sets: Limestone, Stucco, Terracotta, TealPaint, Bronze, Glass, DarkMetal, Timber,
Concrete, Asphalt, Marking, Foliage. Original fixed-seed tileable grain; no stock
images or generative promotional artwork. Most textures are 512²; Concrete and
Asphalt are 1024². BC is sRGB; roughness grayscale and tangent normals are linear;
normal convention is DirectX, no green flip on import.

UE materials `Materials/M_SB205_<material>` reconstruct PBR explicitly; Blender
nodes are not assumed transferable. Metallic/roughness values are distinct in
`generated.json`. Glass is a dark architectural glazing surface, not a rendered
interior or expensive refractive window. M_SB205_CoastalWater is an authored UE
single-layer-water material; no water-physics behaviour is supplied by it.

Hashes, source paths, Blender version, seed, material settings and asset bounds
are recorded in `ArtSource/Buildings/SouthBeach/generated.json`. UE package/source
hashes are in `ArtSource/imported_assets.json`. Map reference changes, retained
actor identities, original materials and rollback information are in
`ArtSource/SouthBeachVisualOverrides.json`.

## Swimming

Editable source: `ArtSource/Animations/Swimming/NeonCitySwimming.blend`;
probe source: `SwimmingProbe.blend`. These contain a working copy of the project's
Epic Quinn reference for authoring. The source reference export under Saved is
not an external download. Original action curves are exported as animation-only
`AN_NC_Swim_Tread`, `Forward`, `Underwater`, `Ascend`, `Descend` FBX files and imported
into `/Game/NeonCity/Animations/Swimming`. Each uses SK_Mannequin directly, 30 fps,
2.4 seconds, 73 sampled keys, looping, root motion disabled.

`ABP_NC_Swimming` and the diagnostic `ABP_NC_Swimming_Probe` are editor-created
assets. The normal player blueprint references the full graph. The original Epic
ABP_Unarmed, skeleton and mesh packages are retained unchanged.

## Phase 20.5 deferred replacement register (historical)

Cars/traffic, speedboat, helicopter, fixed-wing aircraft, weapons, NPC appearances,
background greybox buildings and existing prototype instruction signs remain
their current assets. They are not silently relabelled as finished art. This phase
adds no vehicle type, outfit, weapon, mission, airport or gameplay system.

Artist/user review is still required for material taste, lived-in detail, opaque
glazing, back elevations, swimming deformation and transitions. Passing dimensions
and import validation does not settle those judgements.

## Phase 20.5b additions

| Name/group | Source / author | Licence | Version | Local source | UE destination |
|---|---|---|---|---|---|
| 5 road bodies, 3 wheel styles, police lightbar | NeonCity original procedural/editable geometry | project-owned original; no third-party model | 20.5b / Blender 5.2.2 LTS | ArtSource/FullArt/NeonCityFullArt.blend | Art/FullArt/Vehicles |
| Speedboat and yacht exterior shell | same | same | same | same | Art/FullArt/Marine |
| Helicopter, two rotors, fixed wing | same | same | same | same | Art/FullArt/Aircraft |
| Pistol, SMG, rifle, shotgun | same; no real-brand markings | same | same | same | Art/FullArt/Weapons |
| Beach furniture, palm, utility box, hydrant, meter, street covers | same | same | same | same | Art/FullArt/Beach, Props, Streets |
| 2 building-service sets, shore/foam segments, road repair | NeonCity original | project-owned original | 20.5b | ArtSource/EnvironmentFullArt/SouthBeachEnvironment.blend | Art/FullArt/Buildings, Beach, Streets |
| Mounted gun, cannon, rocket pod, missile rail, guided missile | NeonCity original; existing Phase 19 gameplay | project-owned original | 20.5b | same | Art/FullArt/VehicleWeapons |
| 24 shared finishes | NeonCity original parameter graphs and existing original textures | project-owned original | 20.5b | FullArtGeometry.py, SetupTransportMaterials.py, SetupCoastMaterials.py | Art/FullArt/Materials |
| 5 swimming clip refinements | original authored curves on existing Epic Quinn reference | curves original; underlying Quinn remains Epic licensed project content | 20.5b | ArtSource/Animations/Swimming/NeonCitySwimming.blend | Animations/Swimming |
| Player/NPC color variation | runtime instances of existing Quinn materials | existing Epic template material licence; new palette original | 20.5b | Presentation/CharacterAppearanceComponent | no new character mesh package |

The two source manifests list every FBX, measured bounds, material slots and hash.
The corresponding imported.json ledgers list every UE package/source hash.
See [FullArtInventory.md](FullArtInventory.md) for the measured 41-mesh inventory,
[NeonCityArtBible.md](NeonCityArtBible.md) for budgets, and
[FullArtValidation.md](FullArtValidation.md) for acceptance limits and evidence.
57 new tagged map placements supplement the retained prior reference block.

No paid purchase, web model, brand logo or unknown-source asset was added.
Editable source pieces remain independent Blender objects; export merges copies.
The old primitive/native artwork is retained for fallback and tag-based rollback.
The yacht has exterior and interior-shell planning only, not a completed interior.
Palette variation does not constitute new clothing or a finished character asset.

## Phase 20.5c external intake — licence checked before download

Acquisition and licence verification date: **2026-09-25**. Every adopted source
below is **CC0 1.0 Universal**; attribution is **not required**, but authorship is
retained here voluntarily. These entries supersede the historical statement
above that no web assets were used. Source files remain unmodified in External;
NeonCity derivatives are separate. Exact download URLs, UTC acquisition times,
sizes and SHA-256 hashes are in each source folder's `provenance.json`.

| Asset name | Author | Source website / direct source page | Licence evidence | Local source path | UE destination | Modified / usage / decision |
|---|---|---|---|---|---|---|
| Asphalt 01 | Charlotte Baglioni; Dario Barresi | [Poly Haven asset](https://polyhaven.com/a/asphalt_01) | [Publisher CC0](https://polyhaven.com/license); CC0 1.0 | `ArtSource/External/PolyHaven/asphalt_01/` | `/Game/NeonCity/Art/VerticalSlice/Textures/T_NC205C_asphalt_01_*`; `Materials/M_NC205C_Asphalt` | Modified in UE: world scale, colour balance, roughness/normal control; existing road skins. **FOUND ONLINE AND MODIFIED** |
| Damp Beach Sand | Dimitrios Savva | [Poly Haven asset](https://polyhaven.com/a/damp_beach_sand) | Same publisher CC0; asset structured data also declares CC0 1.0 | `ArtSource/External/PolyHaven/damp_beach_sand/` | `/Game/NeonCity/Art/VerticalSlice/Textures/T_NC205C_damp_beach_sand_*`; `Materials/M_NC205C_Sand` | Modified in UE: measured 2 m scale, dry/wet response, restrained normal strength; shoreline. **FOUND ONLINE AND MODIFIED** |
| Fire Hydrant | Gonçalo Felício | [Poly Haven asset](https://polyhaven.com/a/fire_hydrant) | Same publisher CC0; CC0 1.0 | `ArtSource/External/PolyHaven/fire_hydrant/`; derivative `ArtSource/VerticalSlice/Props/NeonCityLicensedHydrant.blend` | `/Game/NeonCity/Art/VerticalSlice/Props/SM_NC205C_Hydrant`; `Materials/M_NC205C_Hydrant`; matching textures | Modified in Blender/UE: clean variant, UV-preserving reduction to 10,070 triangles, grounded pivot, 0.919 m height, shared colour direction, three LODs; five existing placements. **FOUND ONLINE AND MODIFIED** |
| Universal Animation Library — Standard | Quaternius; animator Gonzalo Furnier | [Author pack](https://quaternius.com/packs/universalanimationlibrary.html), [author's free download](https://quaternius.itch.io/universal-animation-library) | Asset page and ZIP `License.txt`: CC0 1.0; no attribution requirement | `ArtSource/External/Quaternius/UniversalAnimationLibraryStandard/`; derivative `ArtSource/Animations/VerticalSlice/NeonCityLicensedAnimations.blend` | `/Game/NeonCity/Animations/VerticalSlice/AN_NC205C_*` | 43 source actions inspected; 11 selected actions retargeted to Quinn. **FOUND ONLINE AND MODIFIED**: six pistol clips, Hit_Chest and Death01 active in shared character graphs; three other clips still in evaluation at the character checkpoint |

The animation intake uses only the free Standard package, itch upload `17958403`,
archive SHA-256 `cc73fc4e495b82958207316596317a3f40b9fa38065bde1027937452da537724`.
Paid Pro/Source packages were not acquired. The publisher changed the library over
time; the exact acquired binary is identified by its hash rather than an inferred
pack version. Retargeted actions: Pistol_Idle_Loop, Pistol_Aim_Neutral,
Pistol_Aim_Up, Pistol_Aim_Down, Pistol_Reload, Pistol_Shoot, Driving_Loop,
Sitting_Idle_Loop, Hit_Chest, Hit_Head, Death01. Underlying Epic Quinn remains
subject to the existing project/Engine licence; only source Quaternius actions
are CC0. Do not label the combined derivative as wholly CC0.

Coast Sand 05 (Rob Tuytel, Dario Barresi,
[Poly Haven](https://polyhaven.com/a/coast_sand_05), CC0 1.0) was lawfully acquired
on the same date but **BLOCKED BY QUALITY for this beach**: dark compacted gravel
and algae differ from the desired pale fine-grained sand. It has **no UE
destination and no game usage**. It is retained only as a traceable evaluated
source in `ArtSource/External/PolyHaven/coast_sand_05/`.

The two `SM_NC205C_Beach*` meshes are modified project-owned shoreline geometry,
not an external model. Their source provenance is in
`ArtSource/VerticalSlice/Beach/generated.json`; the 6 cm clearance correction
keeps the existing slope and supplies matching static collision.

Licence approval, technical import, and visual acceptance remain separate gates.
Further candidate/category decisions are in [ResourceDiscovery.md](ResourceDiscovery.md).

### Owned transport ergonomic derivatives

`ArtSource/VerticalSlice/Transport/NeonCityCockpitErgonomics.blend` and eight
`SM_NC205C_*` FBXs adapt the existing original Sedan, Sports, SUV, Pickup, Police,
Speedboat, Helicopter and FixedWing models. They add no external model licence.
Changes fit existing Quinn proportions: controls and seats, plane footwell,
simple yoke/lever details and corrected FBX control handedness. These small
original corrections are **CREATED FROM SCRATCH / MODIFIED OWNED SOURCE**;
they are not a claim of finished hero art or an alternative to prior online
category discovery. UE destination: `/Game/NeonCity/Art/VerticalSlice/Transport`.
Source hashes, vertex counts and material slots are in its `generated.json`;
package/source hashes are in `ArtSource/VerticalSlice/imported.json`.

Driving_Loop is now used by `ABP_NC_TransportOccupant`: nine of the eleven
retargeted Library 1 clips are active; Sitting_Idle_Loop and Hit_Head remain
evaluation assets. CC0 source provenance and Epic skeleton restrictions above
continue to apply. No new animation download was needed for seated contact IK.
# Additional intake: Universal Animation Library 2 Standard (evaluation)

- Asset Name: Universal Animation Library 2 Standard, free tier, 43 actions.
- Author: Quaternius; animation collaboration credited to Gonzalo Furnier.
- Source Website: Quaternius / itch.io.
- Direct Source URL: https://quaternius.itch.io/universal-animation-library-2
- License: CC0 1.0 Universal, verified on the publisher's pack page and inside
  the downloaded archive; publisher page explicitly permits commercial projects.
- License Version / Date: CC0 1.0; pack page changelog v2.1, 2026-07-05.
- Attribution Requirement: none under CC0; author credit retained voluntarily.
- Download Date: 2026-09-25. Author's free Standard upload 17958478 only.
- Local Source Path: `ArtSource/External/Quaternius/UniversalAnimationLibrary2Standard/`.
- UE Destination Path: none yet; evaluation only.
- Modified / Unmodified: unmodified source. Only GLB and text notices extracted;
  the additional female mannequin and .blend in the archive were not extracted.
- NeonCity Usage: evaluate get-up, phone, carrying and consumption animation bases.
  Inventory confirms `LayToIdle`, `Idle_TalkingPhone_Loop`, `Walk_Carry_Loop` and
  `Consume`; it does not contain the full advertised paid collection or a rifle set.
- Decision: candidate acquired for evaluation; no runtime adoption claimed.
  SHA-256 and safe file inventory are retained next to the source.

## Adopted base: loafbrr_1 Pistol

- Asset Name: Pistol (digital game model).
- Author: loafbrr_1.
- Source Website: OpenGameArt.
- Direct Source URL: https://opengameart.org/content/pistol-5
- License: CC0 1.0 Universal. The current asset page links CC0 1.0 and the
  actual archive README says `LICENSE: Creative Commons 0`. The page's historical
  correction discussion was reviewed; the downloaded notice is corrected.
- License Version / Date: CC0 1.0; published 2021-12-16, checked 2026-09-25.
- Attribution Requirement: none under CC0; credit retained voluntarily.
- Download Date: 2026-09-25.
- Local Source Path: `ArtSource/External/OpenGameArt/LoafbrrPistol/`.
- UE Destination Path: `/Game/NeonCity/Art/VerticalSlice/Weapons/SM_NC205C_Pistol`,
  five `M_NC205C_Pistol*` materials and Color/Normal/Roughness textures in the same folder.
- Modified / Unmodified: raw source unmodified. Only FBX, PNG and README extracted.
  The supplied Blender file and GLTF are not opened or executed.
- NeonCity Usage: base for the existing NeonCity Pistol. The separate derivative
  `ArtSource/VerticalSlice/Weapons/NeonCityPistol.blend` changes the upper silhouette,
  rear shroud, grip contour, heel, palette and roughness, retaining the source's
  useful topology/UVs. It has no brand marks. Source author credit is retained.
- Modified / integration date: 2026-09-26; source downloaded 2026-09-25 remains
  untouched. The compact 21.59 cm model has 12,785 LOD0 triangles, five material
  slots and three LODs. Grip/palm/support-hand/muzzle sockets fit the shared
  character presentation; original weapon balance and stable IDs remain intact.
- Decision: **FOUND ONLINE AND MODIFIED**. This is one adopted weapon base,
  not completion of the four-weapon art/animation pass. Actual UE captures and
  remaining visual acceptance are recorded in ReviewedPistolValidation.md.
  Archive SHA-256: `3aa9970b4b601fc37a6e6034bfe53f1ebc00fd56cc0a2abb6542abf34e7754f0`.

## Adopted marine engine audio (audible quality acceptance pending)

- Asset Name: boat motor.wav, author-provided public high-quality MP3 preview.
- Author: pawsound.
- Source Website: Freesound.
- Direct Source URL: https://freesound.org/people/pawsound/sounds/154833/
- Direct public preview: https://cdn.freesound.org/previews/154/154833_2592501-hq.mp3
- License: CC0 1.0 Universal, linked by the actual author's asset page.
- License Version / Date: CC0 1.0; published 2012-05-05; verified 2026-09-26 local.
- Attribution Requirement: none under CC0; author credit retained voluntarily.
- Download Date: 2026-09-26 (Asia/Shanghai; UTC receipt retained).
- Local Source Path: `ArtSource/External/Freesound/PawsoundBoatMotor/`.
- UE Destination Path: `/Game/NeonCity/Art/VerticalSlice/Audio/SW_NC205C_BoatEngineLoop`.
- Modified / Unmodified: public MP3 retained unmodified. Separate PCM WAV in
  `ArtSource/VerticalSlice/Audio/` downmixes to mono, excerpts 2–10 seconds, removes
  DC, shapes bandwidth, blends the loop seam and normalizes to -9 dBFS peak.
- NeonCity Usage: spatial engine feedback driven by existing speedboat telemetry.
  Official SoundFactory import passed. Audio-enabled PIE verifies playback starts
  during driving and finishes fading on exit; perceived sound quality is not yet
  approved. Decision: **FOUND ONLINE AND MODIFIED**, with an open audible QA gate.
- The six-channel original WAV requires login and was not requested. The public
  preview is explicitly linked by the same source page; no authentication bypass.
- Source SHA-256: `bd825f65f3261d23b29c0817c093d32477274f9da7a841af9588f00605417fce`.

## Adopted Particle Pack mask (basic water feedback)

- Asset Name: Particle Pack 1.1 (archive version; website release list says 1.0).
- Author: Kenney Vleugels. Additional filter-template credits: Indigo Ray,
  Craig Nisbet, Zoltan Erdokovy, Heliagon, ThreeDee, Killst4r, Tim2501.
- Source Website / Direct Source URL: https://kenney.nl/assets/particle-pack
- License / Version: CC0 1.0 Universal, verified on the publisher page and actual
  downloaded License.txt; attribution optional. Credits retained voluntarily.
- Download Date: 2026-09-26 local; UTC receipt in provenance.json.
- Local Source Path: `ArtSource/External/Kenney/ParticlePack/`.
- UE Destination Path: `/Game/NeonCity/Art/VerticalSlice/Effects/T_NC205C_MarineBreakup`,
  used by the original `M_NC205C_MarineFoam` material in that folder.
- Modified / Unmodified: archive and three reviewed PNG masks unmodified.
- NeonCity Usage: `smoke_01` is the breakup mask for bounded, fading marine
  foam/spray particles. The two other extracted masks remain evaluation only.
  Decision: **FOUND ONLINE AND USED** for the unchanged mask; original material
  and simulation adapt its color, opacity, scale, lifetime and lighting. Official
  import and MarineThrottleLoop05 pass. Actual wake and HUD images were inspected;
  round disconnected stamps in Loop04 were corrected to overlapping narrow trails.
  The larger marine art/ocean visual gate remains open.
- Archive SHA-256: `b631d4b07f7002549fdcf155f01141ad482f79f3440e4e301eed49ce5f1d8958`.

## Adopted aircraft optical head

- Asset Name: Portable Searchlight.
- Author: Elijah Cragg.
- Source Website: Poly Haven.
- Direct Source URL: https://polyhaven.com/a/portable_searchlight
- Actual download: https://dl.polyhaven.org/file/ph-assets/Models/fbx/1k/portable_searchlight/portable_searchlight_1k.fbx
- License: CC0 1.0 Universal, confirmed on the actual asset page and
  https://polyhaven.com/license before downloading.
- License Version / Date: CC0 1.0; published 2026-07-03 per publisher API,
  checked 2026-09-26. Attribution not required; author retained voluntarily.
- Download Date: 2026-09-26 local; UTC receipt retained.
- Local Source Path: `ArtSource/External/PolyHaven/portable_searchlight/`.
- UE Destination Path:
  `/Game/NeonCity/Art/VerticalSlice/AircraftLighting/SM_NC205C_AircraftSearchlight`.
- Modified / Unmodified: raw binary FBX unmodified, publisher MD5/size and local
  SHA-256 verified. No external scripts, source Blend file, textures or website
  preview renders acquired. Separate editable derivative at
  `ArtSource/VerticalSlice/AircraftLighting/NeonCityAircraftSearchlight.blend`.
- NeonCity Usage: only source optical bezel/reflector/lens reused. Original
  lettering, handle, controls and battery case removed. New aircraft housing,
  bearing details, scale and shared palette; 6,282 LOD0 triangles, four slots.
- Decision: **FOUND ONLINE AND MODIFIED**, derivative visually inspected and
  official UE import passed. AircraftSearchlight08 verifies housing/emitter contact,
  bounded aim, actual night light, L on/off, firing while lit and physical landing.
  Five actual 1600 x 900 HUD/world views were inspected. Full aircraft appearance,
  final audio and the larger uninterrupted aviation experience remain open.
- Source SHA-256: `7359afbf460762ffcdbb74432426d95887f2375c2a8ebe94734d2f88eb1ed039`.

## Coastal fishing derivatives — runtime validation in progress

- Asset Name: Fishing Rod.
- Author: Quaternius.
- Source Website: Poly Pizza; [individual asset page](https://poly.pizza/m/skqPvEIpve).
- Direct Source URL: https://static.poly.pizza/10b55863-caee-4ac6-8a53-f9991acb30bb.glb
- License: Public Domain (CC0), explicitly stated on the individual page before
  download. CC0 1.0 Universal; the page does not separately print a version/date.
- License Version / Date: page published 2022-06-20; verified 2026-09-26 local.
- Attribution Requirement: none; author credit retained voluntarily.
- Download Date: 2026-09-26 local; UTC receipt in provenance.json.
- Local Source Path: `ArtSource/External/Quaternius/FishingRod/FishingRod.glb`.
- UE Destination Path: `/Game/NeonCity/Fishing/Art/SM_NC_CoastalRod`.
- Modified / Unmodified: raw GLB immutable. Separate Blender/FBX derivative in
  `ArtSource/VerticalSlice/Fishing/` retains and reshapes the blank, removes old
  guide islands and adds original reel, cork grip, collars and coherent materials.
- NeonCity Usage: visible casting/reeling equipment; authored grip/line sockets.
- Decision: **FOUND ONLINE AND MODIFIED**. Own derivative renders inspected;
  official import/PIE and final visual acceptance are still being verified.
- SHA-256: `e9a09a7b068a6930fa0e2056ca7b667cf3d95aa06a61485a533da837e956fc87`.

- Asset Name: Survival Kit 2.0 — fish, signpost-single, workbench, campfire-stand.
- Author: Kenney.
- Source Website / Direct Source URL: https://kenney.nl/assets/survival-kit
- Actual download: https://kenney.nl/media/pages/assets/survival-kit/4065a8185b-1712149243/kenney_survival-kit.zip
- License: CC0 1.0 Universal, linked by the official page and included License.txt.
- License Version / Date: archive Version 2.0, creation 2024-04-03 per License.txt;
  verified before download on 2026-09-26 local.
- Attribution Requirement: optional; author retained voluntarily.
- Download Date: 2026-09-26 local; UTC receipt retained.
- Local Source Path: `ArtSource/External/Kenney/SurvivalKit/` (immutable archive,
  allowlisted GLBs, required colormap and License.txt; no external script run).
- UE Destination Path: `/Game/NeonCity/Fishing/Art/SM_NC_{Silverfin,CoastBream,CoralSnapper,FishingPoint,FishMarket,FishGrill}`.
- Modified / Unmodified: raw files unchanged. Fish derivatives alter proportions,
  palette/materials and add original surface-mounted eyes/gills/lateral lines.
  Facility bases are rescaled/refinished, with original sign, serving and grill
  surfaces. Raw colormap was used for evaluation only, not a shipped UE texture.
- NeonCity Usage: fictional coastal catches and three existing-region activity
  points. Species are authored variants, not claims of different source geometry.
- Decision: **FOUND ONLINE AND MODIFIED**, with official import/runtime/final art
  validation in progress. Floating-detail/cut-support failures were corrected in
  own Blender reviews. No finished-art approval is inferred from file existence.
- Archive SHA-256: `c3586341b5932c87eb43d75d915434f47daed168b17ed36a03e8ca9977c7443e`.

## Coastal fishing audio — audible acceptance pending

| Required field | Reel | Float / bite contact |
|---|---|---|
| Asset Name | FishingReel.wav public MP3 preview | water splash- small public MP3 preview |
| Author | Harrisando | morganveilleux |
| Source Website | Freesound | Freesound |
| Direct Source URL | https://freesound.org/people/Harrisando/sounds/466205/ | https://freesound.org/people/morganveilleux/sounds/390005/ |
| Actual download | https://cdn.freesound.org/previews/466/466205_9855691-hq.mp3 | https://cdn.freesound.org/previews/390/390005_6987210-hq.mp3 |
| License | CC0 1.0 Universal, linked by individual page before download | CC0 1.0 Universal, linked by individual page before download |
| License Version / Date | CC0 1.0; published 2019-04-12; verified 2026-09-26 | CC0 1.0; published 2017-04-27; verified 2026-09-26 |
| Attribution Requirement | None; author retained voluntarily | None; author retained voluntarily |
| Download Date | 2026-09-26 local; exact UTC receipt retained | 2026-09-26 local; exact UTC receipt retained |
| Local Source Path | ArtSource/External/Freesound/FishingReel/ | ArtSource/External/Freesound/SmallWaterSplash/ |
| UE Destination Path | /Game/NeonCity/Fishing/Audio/SW_NC_FishingReel | /Game/NeonCity/Fishing/Audio/SW_NC_FishingSplash |
| Modified / Unmodified | Raw MP3 unchanged; separate mono PCM 5.14 s excerpt/loop, filtering and peak normalization | Raw MP3 unchanged; separate mono PCM 2.862 s, filtering/fades and peak normalization |
| NeonCity Usage | Bounded local rod reeling playback | Spatial float landing and bite feedback |
| Decision | FOUND ONLINE AND MODIFIED | FOUND ONLINE AND MODIFIED |

Both MP3 signatures and decoded finite PCM bounds were checked. The source pages
explicitly publish these preview URLs; login-only original WAVs were not acquired.
Official SoundFactory imports pass. Signal checks and playback-state tests are
distinct from perceived sound quality, which remains pending. Full provenance,
hashes and derivative operations are retained beside the raw/processed files.

## Tidewater canonical scans and surfaces — converted library, world integration pending

Each row is an individually verified original Poly Haven asset, not an assumption
that the Tidewater MIT license covers every third-party file. The source pack is
locked to `4811ba48d795197de5621985f404e765c0b7c0ef`.

- Source Website: Poly Haven; fixed-version conversion distributed by Tidewater.
- License / version: **CC0 1.0 Universal**, verified on the official asset policy
  and all individual metadata endpoints on 2026-09-26. Original publication dates,
  author roles and raw responses are in `Docs/ThirdParty/TidewaterAssetAudit/`.
- Attribution Requirement: CC0 attribution optional; all author names retained.
  Preserve the Tidewater MIT conversion notice in `Docs/ThirdParty/Tidewater-LICENSE.txt`.
- Download Date: 2026-09-26; exact UTC timestamp in the receipt.
- Local Source Path prefix: `ArtSource/External/Tidewater/PolyHaven/`.
- Direct downloads: every concrete URL is recorded in `provenance.json`, in the
  form `https://raw.githubusercontent.com/dgreenheck/tidewater/4811ba48d795197de5621985f404e765c0b7c0ef/public/models/<path>`.
  All 75 files match their pinned Git blob SHA1 and local SHA256. No upstream script ran.
- Modified / Unmodified: raw source unchanged. Own Blender/FBX conversion preserves
  original pivots, geometry, UVs and normals, changes coordinate/UV conventions,
  retains debris LODs, adds ordinary prop LODs and native UE PBR materials.
  Editable derivatives: `ArtSource/VerticalSlice/Tidewater/Scans/`.
- NeonCity Usage: same-source fishing-village props, coastal debris and architecture
  surfaces. These are library assets; no placement or final parity acceptance yet.
- Decision for all rows: **FOUND ONLINE AND MODIFIED**. Eighteen Blender review
  renders were inspected. The plastic can has a recognizable label and is held
  from world placement until a separately reviewed unbranded derivative is ready.

| Asset Name | Author | Direct Source URL | Local Source Path (prefix above) | UE Destination Path | Remaining gate |
|---|---|---|---|---|---|
| Wooden Crate 02 | James Ray Cock, Jurita Burger | [individual source](https://polyhaven.com/a/wooden_crate_02) | props/props.bin + props/props.json (entry wooden_crate_02) | /Game/NeonCity/Art/Tidewater/Scans/SM_NCTW_wooden_crate_02 | World placement and UE close-view review pending |
| Wooden Crate 01 | James Ray Cock | [individual source](https://polyhaven.com/a/wooden_crate_01) | props/props.bin + props/props.json (entry wooden_crate_01) | /Game/NeonCity/Art/Tidewater/Scans/SM_NCTW_wooden_crate_01 | World placement and UE close-view review pending |
| Wooden Bucket 01 | James Ray Cock | [individual source](https://polyhaven.com/a/wooden_bucket_01) | props/props.bin + props/props.json (entry wooden_bucket_01) | /Game/NeonCity/Art/Tidewater/Scans/SM_NCTW_wooden_bucket_01 | World placement and UE close-view review pending |
| Fish Knife | Mateusz Sadek | [individual source](https://polyhaven.com/a/fish_knife) | props/props.bin + props/props.json (entry fish_knife) | /Game/NeonCity/Art/Tidewater/Scans/SM_NCTW_fish_knife | World placement and UE close-view review pending |
| Wooden Cutting Board | Kuutti Siitonen | [individual source](https://polyhaven.com/a/wooden_cutting_board) | props/props.bin + props/props.json (entry wooden_cutting_board) | /Game/NeonCity/Art/Tidewater/Scans/SM_NCTW_wooden_cutting_board | World placement and UE close-view review pending |
| Lifebuoy | Hank Kaamura | [individual source](https://polyhaven.com/a/lifebuoy) | props/props.bin + props/props.json (entry lifebuoy) | /Game/NeonCity/Art/Tidewater/Scans/SM_NCTW_lifebuoy | World placement and UE close-view review pending |
| Wooden Lantern 01 | James Ray Cock | [individual source](https://polyhaven.com/a/wooden_lantern_01) | props/props.bin + props/props.json (entry wooden_lantern_01) | /Game/NeonCity/Art/Tidewater/Scans/SM_NCTW_wooden_lantern_01 | World placement and UE close-view review pending |
| Fishermans Hat | PierreB3D | [individual source](https://polyhaven.com/a/fishermans_hat) | props/props.bin + props/props.json (entry fishermans_hat) | /Game/NeonCity/Art/Tidewater/Scans/SM_NCTW_fishermans_hat | World placement and UE close-view review pending |
| Wooden Table 03 | Gabriel Radić | [individual source](https://polyhaven.com/a/WoodenTable_03) | props/props.bin + props/props.json (entry WoodenTable_03) | /Game/NeonCity/Art/Tidewater/Scans/SM_NCTW_WoodenTable_03 | World placement and UE close-view review pending |
| Metal Jerrycan Green | Ulan Cabanilla | [individual source](https://polyhaven.com/a/metal_jerrycan_green) | props/props.bin + props/props.json (entry metal_jerrycan_green) | /Game/NeonCity/Art/Tidewater/Scans/SM_NCTW_metal_jerrycan_green | World placement and UE close-view review pending |
| Plastic Jerrycan | Ulan Cabanilla | [individual source](https://polyhaven.com/a/plastic_jerrycan) | props/props.bin + props/props.json (entry plastic_jerrycan) | /Game/NeonCity/Art/Tidewater/Scans/SM_NCTW_plastic_jerrycan | Remove recognizable can label before placement |
| Life Jacket | PierreB3D | [individual source](https://polyhaven.com/a/life_jacket) | props/props.bin + props/props.json (entry life_jacket) | /Game/NeonCity/Art/Tidewater/Scans/SM_NCTW_life_jacket | World placement and UE close-view review pending |
| Metal Toolbox | Mateusz Sadek | [individual source](https://polyhaven.com/a/metal_toolbox) | props/props.bin + props/props.json (entry metal_toolbox) | /Game/NeonCity/Art/Tidewater/Scans/SM_NCTW_metal_toolbox | World placement and UE close-view review pending |
| Wooden Display Shelves 01 | James Ray Cock | [individual source](https://polyhaven.com/a/wooden_display_shelves_01) | props/props.bin + props/props.json (entry wooden_display_shelves_01) | /Game/NeonCity/Art/Tidewater/Scans/SM_NCTW_wooden_display_shelves_01 | World placement and UE close-view review pending |
| Weathered Brown Planks | Dimitrios Savva, Rico Cilliers | [individual source](https://polyhaven.com/a/weathered_brown_planks) | props/s_weathered_brown_planks_{a,n,r}.jpg | /Game/NeonCity/Art/Tidewater/Materials/M_NCTW_weathered_brown_planks | World placement and UE close-view review pending |
| Weathered Planks | Dario Barresi, Dimitrios Savva | [individual source](https://polyhaven.com/a/weathered_planks) | props/s_weathered_planks_{a,n,r}.jpg | /Game/NeonCity/Art/Tidewater/Materials/M_NCTW_weathered_planks | World placement and UE close-view review pending |
| Worn Corrugated Iron | Jenelle van Heerden, Dimitrios Savva | [individual source](https://polyhaven.com/a/worn_corrugated_iron) | props/s_worn_corrugated_iron_{a,n,r}.jpg | /Game/NeonCity/Art/Tidewater/Materials/M_NCTW_worn_corrugated_iron | World placement and UE close-view review pending |
| Weathered Peeling Timber | Dimitrios Savva | [individual source](https://polyhaven.com/a/weathered_peeling_timber) | props/s_weathered_peeling_timber_{a,n,r}.jpg | /Game/NeonCity/Art/Tidewater/Materials/M_NCTW_weathered_peeling_timber | World placement and UE close-view review pending |
| Dead Quiver Trunk | James Ray Cock, Dario Barresi, Rico Cilliers | [individual source](https://polyhaven.com/a/dead_quiver_trunk) | debris/dead_quiver_trunk.glb | /Game/NeonCity/Art/Tidewater/Scans/SM_NCTW_dead_quiver_trunk | World placement and UE close-view review pending |
| Dead Quiver Branch 01 | Greg Zaal, Jenelle van Heerden | [individual source](https://polyhaven.com/a/dead_quiver_branch_01) | debris/dead_quiver_branch_01.glb | /Game/NeonCity/Art/Tidewater/Scans/SM_NCTW_dead_quiver_branch_01 | World placement and UE close-view review pending |
| Dead Quiver Branch 02 | Greg Zaal, Jenelle van Heerden | [individual source](https://polyhaven.com/a/dead_quiver_branch_02) | debris/dead_quiver_branch_02.glb | /Game/NeonCity/Art/Tidewater/Scans/SM_NCTW_dead_quiver_branch_02 | World placement and UE close-view review pending |
| Lambis Shell | Kuutti Siitonen | [individual source](https://polyhaven.com/a/lambis_shell) | debris/lambis_shell.glb | /Game/NeonCity/Art/Tidewater/Scans/SM_NCTW_lambis_shell | World placement and UE close-view review pending |

## Rocketbox coastal vendors and gameplay people

- Asset Names: `Wood_Male_01`, `Female_Adult_04`, each with seven source idle,
  observation, talk, wave and shrug animations.
- Author / Source Website: Microsoft Rocketbox Avatar Library;
  [official repository](https://github.com/microsoft/Microsoft-Rocketbox).
- Direct Source URLs: original assets under
  `Assets/Avatars/Professions/Wood_Male_01/` and
  `Assets/Avatars/Adults/Female_Adult_04/` at official commit
  `0943055db6ec570bcef9f2c8b41c9e5467c808f9`. Exact 2 avatar / 14 animation paths
  are recorded in `Docs/ThirdParty/TidewaterAssetAudit/Rocketbox/Audit.json`.
- Actual downloads: fixed Tidewater conversions
  [male avatar](https://raw.githubusercontent.com/dgreenheck/tidewater/4811ba48d795197de5621985f404e765c0b7c0ef/public/models/characters/joe.glb)
  and [female avatar](https://raw.githubusercontent.com/dgreenheck/tidewater/4811ba48d795197de5621985f404e765c0b7c0ef/public/models/characters/marta.glb).
- License / Version / Date: MIT, Copyright (c) 2020 Microsoft; actual pinned
  primary `LICENSE.md` verified 2026-09-26, exact copy retained. Complete Git tree
  has no nested avatar/animation license override. The November 2020 license
  update in the official README supersedes its older research-only announcement.
- Attribution Requirement: retain complete Microsoft MIT copyright/license and
  the Tidewater conversion MIT notice with distributed copies.
- Download Date: 2026-09-26; exact UTC receipt retained.
- Local Source Path: `ArtSource/External/Tidewater/Rocketbox/`.
- UE Destination Path: `/Game/NeonCity/Art/Tidewater/Characters/<original asset ID>/`.
  Each contains `SK_NCTW_<ID>`, its Skeleton and seven `AN_NCTW_<ID>_<clip>`
  sequences, PBR textures/materials and three mesh LODs. Exact paths are recorded
  per asset in the raw receipt and `Characters/Import.json`.
  Gameplay derivatives are `/Game/NeonCity/Art/Tidewater/CharacterBindings/`
  `SK_NCTW_Wood_Male_01_Gameplay` and `SK_NCTW_Female_Adult_04_Gameplay`.
  Their editable sources and conversion/import receipts are in
  `ArtSource/VerticalSlice/Tidewater/CharacterBindings/`.
- Modified / Unmodified: raw GLB bytes unchanged and pinned blob/hash checked;
  only local filenames use original avatar IDs. No downloaded tool was executed.
  Each GLB has seven embedded animations and no external-resource URLs.
- NeonCity Usage: player, pedestrian appearances and their existing visible
  transport occupants. Fishing-village vendor interactions remain separate work.
  Upstream names are provenance only.
- Decision: **FOUND ONLINE AND MODIFIED**. Editable Blender/FBX derivatives
  preserve the original skin and clips; embedded textures retain their exact bytes.
  Original bind pose and source alpha-test semantics are retained. Blender pose
  frames were inspected; native review and vendor gameplay acceptance are separate.
  Microsoft and Tidewater notices are included in binary staging rules.
  Separate gameplay skins retain original topology/UVs/textures and adapt bind
  pose and skin weights to the existing project skeleton. Body/shoulder weights
  and a rigid head offset preserve usable existing movement/combat/seat poses.
  Existing animations, physics and gameplay remain in use. Details and actual
  normal-game acceptance are in `Docs/ReferenceCharacterValidation.md`.

## Tidewater canonical audio — individually cleared source recordings

- Source Website: Freesound, with locked Tidewater mixes served by the canonical repository.
- License: **CC0 1.0 Universal**, verified on every actual recording page on
  2026-09-26, including recursively cited constituents of mixed recordings.
  `Docs/ThirdParty/TidewaterAssetAudit/Audio/Audit.json` preserves 53 primary
  page records, full HTML, author/title, license links and SHA256 evidence.
- Attribution Requirement: CC0 credits optional; retain all source and mix
  authors. The source bank/slice-map code is MIT; its notice stays in the project.
- Download Date: 2026-09-26T02:47:33.720189+00:00.
- Direct download URLs and both Git blob SHA1 / SHA256 are recorded per file in
  `ArtSource/External/Tidewater/Audio/provenance.json`. All 43 binary files match
  the pinned source commit. File signatures were checked before decoding.
- Modified / Unmodified: downloaded Opus recordings unchanged. Native PCM
  conversion preserves source loop timing, original sprite cuts and levels.
- Decision: **FOUND ONLINE AND MODIFIED** through native container/slice conversion.
  The imported library still requires runtime mixing and listening acceptance.
- NeonCity Usage: the following canonical marine, environment and fishing cues.
  Fourteen loops and 130 exact source sprite cuts are native SoundWaves.
  Runtime mixer and listening review remain open; exact per-cue paths, source
  intervals and LUFS are retained in the generated bank and raw receipt.

| Asset Name | Author (all mix constituents) | Direct Source URL | Local Source Path | UE Destination Path | NeonCity Usage |
|---|---|---|---|---|---|
| surf_crash.ogg | YevgVerh, straget | [source 1](https://freesound.org/people/YevgVerh/sounds/827530/), [source 2](https://freesound.org/people/straget/sounds/412308/) | ArtSource/External/Tidewater/Audio/surf_crash.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_surf_crash_01..07 | 7 breaking waves (crash / thump) — sand beach + rough shore; played per wave at its break point |
| surf_wash.ogg | Alex_hears_things | [source 1](https://freesound.org/people/Alex_hears_things/sounds/352356/) | ArtSource/External/Tidewater/Audio/surf_wash.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_surf_wash_01..06 | 6 uprushes (swash running up the sand), close mic |
| surf_backwash.ogg | Alex_hears_things | [source 1](https://freesound.org/people/Alex_hears_things/sounds/352356/) | ArtSource/External/Tidewater/Audio/surf_backwash.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_surf_backwash_01..06 | 6 backwash drains (sand fizz), close mic |
| surf_far.ogg | chris_dagorne | [source 1](https://freesound.org/people/chris_dagorne/sounds/426074/) | ArtSource/External/Tidewater/Audio/surf_far.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_surf_far | distant surf roar (cliff top) — loop |
| wind.ogg | bruno.auzet | [source 1](https://freesound.org/people/bruno.auzet/sounds/706471/) | ArtSource/External/Tidewater/Audio/wind.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_wind | coastal wind through bushes — loop, gust-modulated |
| palms.ogg | felix.blume | [source 1](https://freesound.org/people/felix.blume/sounds/384550/) | ArtSource/External/Tidewater/Audio/palms.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_palms | wind in palm trees (+ day insects) — loop, gust-modulated |
| crickets.ogg | hdfreema | [source 1](https://freesound.org/people/hdfreema/sounds/333221/) | ArtSource/External/Tidewater/Audio/crickets.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_crickets | night crickets — loop |
| pier_lap.ogg | richardemoore | [source 1](https://freesound.org/people/richardemoore/sounds/260263/) | ArtSource/External/Tidewater/Audio/pier_lap.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_pier_lap | water lapping a dock — loop |
| under_reef.ogg | KaleidacousticsAudio | [source 1](https://freesound.org/people/KaleidacousticsAudio/sounds/630436/) | ArtSource/External/Tidewater/Audio/under_reef.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_under_reef | hydrophone in a cove: snapping-shrimp crackle, low rumble — loop |
| boat_engine.ogg | AugustSandberg | [source 1](https://freesound.org/people/AugustSandberg/sounds/264864/) | ArtSource/External/Tidewater/Audio/boat_engine.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_boat_engine | Volvo Penta MD22 marine diesel — loop, pitched with rpm |
| boat_lap.ogg | kyles | [source 1](https://freesound.org/people/kyles/sounds/637206/) | ArtSource/External/Tidewater/Audio/boat_lap.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_boat_lap | water lapping a fibreglass hull — loop |
| hull_slap.ogg | kyles | [source 1](https://freesound.org/people/kyles/sounds/637206/) | ArtSource/External/Tidewater/Audio/hull_slap.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_hull_slap_01..05 | 5 water knocks on the hull |
| boat_rush.ogg | Pfannkuchn | [source 1](https://freesound.org/people/Pfannkuchn/sounds/360631/) | ArtSource/External/Tidewater/Audio/boat_rush.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_boat_rush | bow wave / water past the hull — loop |
| step_sand.ogg | 200221-WeanBekker | [source 1](https://freesound.org/people/200221-WeanBekker/sounds/543714/) | ArtSource/External/Tidewater/Audio/step_sand.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_step_sand_01..05 | 5 steps, soft loose dry sand (foley, no surf) |
| step_wetsand.ogg | bruno.auzet | [source 1](https://freesound.org/people/bruno.auzet/sounds/539175/) | ArtSource/External/Tidewater/Audio/step_wetsand.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_step_wetsand_01..05 | 5 steps, wet coarse sand (close) |
| step_wood.ogg | florianreichelt | [source 1](https://freesound.org/people/florianreichelt/sounds/459970/) | ArtSource/External/Tidewater/Audio/step_wood.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_step_wood_01..05 | 5 steps, hollow wood |
| step_water.ogg | Nox_Sound, ryansitz | [source 1](https://freesound.org/people/Nox_Sound/sounds/490951/), [source 2](https://freesound.org/people/ryansitz/sounds/342932/) | ArtSource/External/Tidewater/Audio/step_water.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_step_water_01..05 | 5 wading swishes (slowest-attack picks) |
| step_grass.ogg | SilentStrikeZ | [source 1](https://freesound.org/people/SilentStrikeZ/sounds/389625/) | ArtSource/External/Tidewater/Audio/step_grass.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_step_grass_01..05 | 5 steps, grass |
| step_rock.ogg | SecureSubset | [source 1](https://freesound.org/people/SecureSubset/sounds/813622/) | ArtSource/External/Tidewater/Audio/step_rock.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_step_rock_01..05 | 5 steps, stone |
| swim.ogg | tbsounddesigns | [source 1](https://freesound.org/people/tbsounddesigns/sounds/530158/) | ArtSource/External/Tidewater/Audio/swim.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_swim_01..04 | 4 surface swim strokes |
| uw_swim.ogg | felix.blume | [source 1](https://freesound.org/people/felix.blume/sounds/384218/) | ArtSource/External/Tidewater/Audio/uw_swim.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_uw_swim_01..04 | 4 underwater hand/arm movements (hydrophone) |
| submerge.ogg | felix.blume | [source 1](https://freesound.org/people/felix.blume/sounds/384218/) | ArtSource/External/Tidewater/Audio/submerge.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_submerge_01..02 | 2 short underwater water-rushes (hydrophone) |
| emerge.ogg | felix.blume | [source 1](https://freesound.org/people/felix.blume/sounds/434978/) | ArtSource/External/Tidewater/Audio/emerge.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_emerge_01 | water settling / dripping (short) |
| splash.ogg | Nox_Sound, blaukreuz, qubodup | [source 1](https://freesound.org/people/Nox_Sound/sounds/585744/), [source 2](https://freesound.org/people/blaukreuz/sounds/195877/), [source 3](https://freesound.org/people/qubodup/sounds/212143/) | ArtSource/External/Tidewater/Audio/splash.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_splash_01..04 | 4 splashes (body into water) |
| gull.ogg | Soojay, squashy555 | [source 1](https://freesound.org/people/Soojay/sounds/462462/), [source 2](https://freesound.org/people/squashy555/sounds/353416/) | ArtSource/External/Tidewater/Audio/gull.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_gull_01..06 | 6 gull calls |
| bird_forest.ogg | coalcon, dubminister | [source 1](https://freesound.org/people/coalcon/sounds/636071/), [source 2](https://freesound.org/people/coalcon/sounds/636072/), [source 3](https://freesound.org/people/dubminister/sounds/214676/) | ArtSource/External/Tidewater/Audio/bird_forest.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_bird_forest_01..13 | 13 songbird calls / short songs (high-passed 1.1 kHz); sung in bouts from perches in the trees |
| bird_dove.ogg | nathankwright | [source 1](https://freesound.org/people/nathankwright/sounds/456930/) | ArtSource/External/Tidewater/Audio/bird_dove.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_bird_dove_01..05 | 5 dove coo phrases |
| birds_dawn.ogg | SorenF109 | [source 1](https://freesound.org/people/SorenF109/sounds/678859/) | ArtSource/External/Tidewater/Audio/birds_dawn.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_birds_dawn | dawn chorus (Waikoloa, Hawaii, morning) — loop |
| tern.ogg | LaScienceMusicale | [source 1](https://freesound.org/people/LaScienceMusicale/sounds/648386/) | ArtSource/External/Tidewater/Audio/tern.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_tern_01..06 | 6 common tern calls (high-passed 2 kHz) |
| whale_song.ogg | KEVOY | [source 1](https://freesound.org/people/KEVOY/sounds/82325/) | ArtSource/External/Tidewater/Audio/whale_song.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_whale_song | humpback whale song, recorded diving (French Polynesia), low-passed 2.6 kHz (divers' regulator hiss) — loop |
| whale_blow.ogg | mikewest | [source 1](https://freesound.org/people/mikewest/sounds/198937/) | ArtSource/External/Tidewater/Audio/whale_blow.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_whale_blow_01..03 | 3 cetacean breaths (explosive exhale + inhale), played pitched down as the humpback's blow |
| big_splash.ogg | blaukreuz, qubodup, roboroo, wormer2 | [source 1](https://freesound.org/people/blaukreuz/sounds/195877/), [source 2](https://freesound.org/people/qubodup/sounds/212143/), [source 3](https://freesound.org/people/qubodup/sounds/442773/), [source 4](https://freesound.org/people/roboroo/sounds/436792/), [source 5](https://freesound.org/people/wormer2/sounds/415669/), [source 6](https://freesound.org/people/wormer2/sounds/415670/) | ArtSource/External/Tidewater/Audio/big_splash.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_big_splash_01..02 | 2 large splashes (whale breach / re-entry / flukes, pitched down) |
| reel_wind.ogg | tosha73 | [source 1](https://freesound.org/people/tosha73/sounds/509902/) | ArtSource/External/Tidewater/Audio/reel_wind.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_reel_wind | spinning reel cranked steadily (gear ticking) — loop, rate follows the crank |
| reel_drag.ogg | paulprit | [source 1](https://freesound.org/people/paulprit/sounds/507070/) | ArtSource/External/Tidewater/Audio/reel_drag.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_reel_drag | reel ratchet screaming as line is pulled off (a fish running against the drag) — loop |
| line_strain.ogg | kyles | [source 1](https://freesound.org/people/kyles/sounds/450849/) | ArtSource/External/Tidewater/Audio/line_strain.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_line_strain | nylon line under strain (creak near the breaking point) — loop |
| rod_swish.ogg | Mrthenoronha, mwchristian95 | [source 1](https://freesound.org/people/Mrthenoronha/sounds/371313/), [source 2](https://freesound.org/people/mwchristian95/sounds/725426/) | ArtSource/External/Tidewater/Audio/rod_swish.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_rod_swish_01..05 | 5 rod swishes (the cast) |
| bail_click.ogg | MrFossy | [source 1](https://freesound.org/people/MrFossy/sounds/523282/) | ArtSource/External/Tidewater/Audio/bail_click.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_bail_click_01..04 | 4 small sprung-metal clicks (the bail opening / snapping shut) |
| line_out.ogg | BranndyBottle | [source 1](https://freesound.org/people/BranndyBottle/sounds/464697/) | ArtSource/External/Tidewater/Audio/line_out.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_line_out_01 | line paying out off the reel during a cast |
| plop.ogg | BranndyBottle, JoelMcDaniel | [source 1](https://freesound.org/people/BranndyBottle/sounds/464697/), [source 2](https://freesound.org/people/JoelMcDaniel/sounds/849752/) | ArtSource/External/Tidewater/Audio/plop.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_plop_01..02 | 2 bobber / sinker landings |
| line_snap.ogg | khenshom | [source 1](https://freesound.org/people/khenshom/sounds/537084/) | ArtSource/External/Tidewater/Audio/line_snap.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_line_snap_01..04 | 4 snaps (strings breaking under tension, used for the line parting) |
| fish_splash.ogg | paulprit | [source 1](https://freesound.org/people/paulprit/sounds/507093/), [source 2](https://freesound.org/people/paulprit/sounds/507094/) | ArtSource/External/Tidewater/Audio/fish_splash.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_fish_splash_01..05 | 5 fish splashes / thrashing at the surface |
| fish_flop.ogg | RatBird, ramattahatta | [source 1](https://freesound.org/people/RatBird/sounds/570208/), [source 2](https://freesound.org/people/ramattahatta/sounds/649003/) | ArtSource/External/Tidewater/Audio/fish_flop.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_fish_flop_01..04 | landed fish flapping / flopping |
| coins.ogg | Anthousai | [source 1](https://freesound.org/people/Anthousai/sounds/336585/) | ArtSource/External/Tidewater/Audio/coins.ogg | /Game/NeonCity/Art/Tidewater/Audio/SW_NCTW_coins_01 | coins dropped in a bowl (selling fish) |


## Tidewater canonical working boat — Harborline 82

- Asset Name: locked Tidewater 8.2 m working fishing boat, including wheelhouse,
  deck equipment, controls, radar, propeller, rudder and seven surface programs.
- Author: DRG Software Solutions LLC, 2026.
- Source Website: canonical Tidewater GitHub repository.
- Direct Source URL: https://github.com/dgreenheck/tidewater/blob/4811ba48d795197de5621985f404e765c0b7c0ef/src/world/BoatModel.js
  and its pinned `src/world/boat/` modules; per-module hashes in `Boat/Source.json`.
- License / Version / Date: MIT, copyright 2026; exact pinned notice verified and
  retained in `Docs/ThirdParty/Tidewater-LICENSE.txt` before conversion.
- Attribution Requirement: retain copyright and complete MIT permission notice
  with distributed copies. Runtime staging already includes this notice.
- Download Date: locked source acquired 2026-09-26; geometry exported locally from
  the reviewed CPU constructor, with no new external binary or script download.
- Local Source Path: `ThirdPartyReference/Tidewater/src/world/BoatModel.js` and
  `ArtSource/VerticalSlice/Tidewater/Boat/` (immutable arrays, Blender, FBX, shaders,
  geometry and hydrodynamics receipts).
- UE Destination Path: `/Game/NeonCity/Art/Tidewater/Boat/`; seven materials and
  twelve `SM_NCTW_FishingBoat_*` assets. Exact paths/hashes in `Boat/Import.json`.
- Modified / Unmodified: source arrays unchanged; native coordinate/UV conversion,
  source-neutral rotation baking with retained pivots, three LODs, UE lighting
  adaptation and source contact geometry. The isolated helm chair moves 40 cm
  toward the wheel for native seated reach (1,559 vertices; all triangles kept);
  its collider and body/eye anchors move consistently. No existing speedboat mesh
  is replaced.
- NeonCity Usage: Harborline 82 walkable fishing boat and shared marine camera/
  physics integration. Full player parity remains under validation.
- Decision: **FOUND ONLINE AND MODIFIED**. Faithful licensed source conversion
  retains mature geometry/material detail; no lower-quality primitive substitute.
- Evidence and open differences: `Docs/MarineReferenceBoatValidation.md`.

## Tidewater canonical fishing village and pier

- Asset Name: seeded canonical village buildings, market, boardwalk, pier,
  fittings, hanging signs/lanterns and source procedural surface programs.
- Author: DRG Software Solutions LLC, 2026.
- Source Website: official Tidewater GitHub repository.
- Direct Source URL: https://github.com/dgreenheck/tidewater/blob/4811ba48d795197de5621985f404e765c0b7c0ef/src/world/Village.js
  and pinned `Pier.js`, `Props.js`, `world/village/` modules. Exact source paths
  and hashes are recorded in `Village/Source.json`.
- License / Version / Date: MIT, copyright 2026; verified locked notice in
  `Docs/ThirdParty/Tidewater-LICENSE.txt`.
- Attribution Requirement: retain copyright and full MIT permission notice with
  copies or substantial portions; notice is staged for distribution.
- Download Date: source acquired 2026-09-26. Geometry and shaders extracted from
  the reviewed local CPU constructors; no new installer or unknown script used.
- Local Source Path: `ThirdPartyReference/Tidewater/src/world/village/` and
  `ArtSource/VerticalSlice/Tidewater/Village/` (immutable arrays, editable Blender,
  FBX, shaders and bake receipts).
- UE Destination Path: `/Game/NeonCity/Art/Tidewater/Village/`; 31 visual meshes,
  one collision proxy, fourteen textures and seven materials. Actual package
  paths/hashes are recorded in `Village/Import.json` and the shared asset ledger;
  the source-layout actor is placed in the existing native island sublevel.
- Modified / Unmodified: canonical source geometry/layout unchanged; conversion
  to UE coordinates, five UV channels and linear colours; three LODs; native
  lighting and source-equation material bakes; source analytic collision.
- NeonCity Usage: fishing village in the canonical island sublevel. Fish-stall
  creatures, CC0 scanned props and native vendor integration remain separate
  pending work, and are not claimed by this entry.
- Decision: **FOUND ONLINE AND MODIFIED**. Uses licensed canonical geometry and
  surface detail instead of a lower-quality newly invented replacement.
- Evidence and remaining differences: `Docs/MarineReferenceVillageValidation.md`.

## Tidewater canonical vegetation source conversion

- Asset Name: canonical tropical vegetation geometry, exact seeded scatter,
  leaf-cluster atlas, dune/grass mask, plant material/wind and canopy LOD programs.
- Author: DRG Software Solutions LLC, 2026.
- Source Website: official Tidewater GitHub repository.
- Direct Source URL: https://github.com/dgreenheck/tidewater/blob/4811ba48d795197de5621985f404e765c0b7c0ef/src/world/Vegetation.js
  and its pinned `world/vegetation/` modules; exact file hashes in `Source.json`.
- License / Version / Date: MIT, copyright 2026; locked full notice retained in
  `Docs/ThirdParty/Tidewater-LICENSE.txt` and staged with distribution.
- Attribution Requirement: retain copyright and full MIT permission notice.
- Download Date: reference acquired 2026-09-26; this conversion reads the reviewed
  local source, with no new external executable or asset download.
- Local Source Path: `ThirdPartyReference/Tidewater/src/world/vegetation/`;
  `ArtSource/VerticalSlice/Tidewater/Vegetation/` holds immutable source records,
  editable Blender geometry, FBX derivatives and native shader ports.
- UE Destination Path: `/Game/NeonCity/Art/Tidewater/Vegetation/`: twelve static
  meshes, four source-data textures, three native materials and one scatter Data
  Asset. The existing island level contains all 65,383 source plants in thirteen
  near HISM groups, plus five far representation groups for the same 18,343
  trees/shrubs and one far group for the same 233 palms; package and map receipts
  are recorded separately. The original PalmFar source mesh is reused.
- Modified / Unmodified: exact ten-family scatter and original geometry/morph
  attributes retained; coordinates/UVs adapted for UE; source equations converted
  to HLSL and native two-sided foliage lighting. Crown variant and all 180 baked
  impostor views come from the original implementation. Native minified-coverage
  cutoff and optional thinning preserve forest silhouette; the user's hill-leaf
  colour adjustment is shared by near/far materials. Source shadow policy is
  retained: distant cards do not cast dynamic shadows. No source models changed.
- NeonCity Usage: island forest canopy, beach and hillside flora. Native world
  placement and native ground flora are implemented; forest daylight balance,
  complete-island performance and continuous player validation remain open.
- Decision: **FOUND ONLINE AND MODIFIED**. The original MIT procedural models
  and shaders are the source assets; no replacement cylinder/sphere trees.
- Evidence and open gates: `Docs/MarineReferenceVegetationValidation.md`.

### User-requested mountain jungle density adaptation — 2026-09-26

- Asset Name / Author: Tidewater tree, shrub, fern and monstera geometry; DRG
  Software Solutions LLC, with NeonCity-authored supplemental placement.
- Source Website / Direct URL: official source repository,
  https://github.com/dgreenheck/tidewater/tree/4811ba48d795197de5621985f404e765c0b7c0ef/src/world/vegetation
- License / Date / Attribution: MIT, copyright 2026; preserve the full notice
  `Docs/ThirdParty/Tidewater-LICENSE.txt`, already included in distribution.
- Download Date: no new download; uses the reviewed 2026-09-26 source intake.
- Local Source Path: `ArtSource/VerticalSlice/Tidewater/Vegetation/` and
  `ArtSource/VerticalSlice/Tidewater/Jungle/` (seeded authored placement and hashes).
- UE Destination Path: `/Game/NeonCity/Art/Tidewater/Vegetation/DA_NCTW_MountainJungle`
  and a separate tagged actor in the existing native Tidewater island map.
- Modified / Unmodified: original source geometry retained; native canopy meshes
  use Nanite Preserve Area and preserve semantic UVs. Native materials add camera
  clearance and root-anchored contact springs after the unchanged source wind.
  The 29,035 supplemental plants have native terrain grounding, overlapping mature crowns
  and denser trail understory. The 65,383 canonical source plants are retained.
- NeonCity Usage: continuous-looking mountain forest and a sheltered hiking trail.
- Decision: **FOUND ONLINE AND MODIFIED**. This is a user-requested density
  adaptation of reviewed source assets, not an additional model acquisition.
- Validation and limitations: `Docs/MountainJungleValidation.md`.

### Mountain tree wood collision derivative — 2026-09-26

- Asset Name: Tidewater broadleaf tree bark-only collision mesh.
- Author: DRG Software Solutions LLC; NeonCity extraction/conversion.
- Source Website: official Tidewater GitHub repository.
- Direct Source URL: https://github.com/dgreenheck/tidewater/blob/4811ba48d795197de5621985f404e765c0b7c0ef/src/world/vegetation/PlantGeometry.js
- License / Version / Date: MIT, copyright 2026; full notice retained and staged.
- Attribution Requirement: preserve copyright and MIT permission notice.
- Download Date: no new download; uses the reviewed 2026-09-26 intake.
- Local Source Path: `ArtSource/VerticalSlice/Tidewater/FoliageInteraction/`;
  editable `.blend`, `.fbx`, generated receipt, original geometry hash.
- UE Destination Path: `/Game/NeonCity/Art/Tidewater/Vegetation/SM_NCTW_TreeWoodCollision`.
- Modified / Unmodified: extracted 638 original bark triangles / 526 vertices,
  verified identical across the three source variants; leaves excluded. Native
  double-sided query/kinematic collision added; original visible geometry intact.
- NeonCity Usage: actual nearby character and vehicle contact with mountain trees.
- Decision: **FOUND ONLINE AND MODIFIED**. Source-exact bark avoids an oversized
  primitive crown collider. No new downloaded asset or unknown script is executed.
- Validation and limits: `Docs/FoliageInteractionValidation.md`.

### Canonical grass, sea oats and beach creeper — 2026-09-26

- Asset Name: Tidewater GrassField source patch meshes, density/height data and
  material/wind equations.
- Author: DRG Software Solutions LLC, copyright 2026.
- Source Website: official Tidewater GitHub repository.
- Direct Source URL: https://github.com/dgreenheck/tidewater/blob/4811ba48d795197de5621985f404e765c0b7c0ef/src/world/vegetation/GrassField.js
- License / Version / Date: MIT, copyright 2026; locked full notice retained.
- Attribution Requirement: retain copyright and MIT permission notice; already
  staged through `Docs/ThirdParty/Tidewater-LICENSE.txt`.
- Download Date: no new download; reviewed source intake from 2026-09-26.
- Local Source Path: `ThirdPartyReference/Tidewater/src/world/vegetation/` and
  `ArtSource/VerticalSlice/Tidewater/Grass/` (source buffers, editable Blender,
  FBX, HLSL ports, exact native height texture and receipts).
- UE Destination Path: `/Game/NeonCity/Art/Tidewater/Grass/`; the existing island's
  canonical vegetation owner supplies its single runtime patch component.
- Modified / Unmodified: original patch topology, clump identities, density,
  species, tier ranges/fades and wind equations retained; coordinates, packed
  shader attributes and native material graph adapted. Ground height samples
  match the actual UE landscape triangles rather than the source bilinear field.
- NeonCity Usage: island meadow, dunes, sea oats and beach ground flora.
- Decision: **FOUND ONLINE AND MODIFIED**. Uses the reviewed source implementation
  rather than independently inventing lower-detail grass geometry.
- Evidence / open gates: `Docs/ReferenceGrassValidation.md`; first native walk
  passes, but colour/performance and the complete route are not yet accepted.

### Coconut-palm contact adapter — 2026-09-26

- Asset Name: Tidewater coconut stem curve and radius profile, native collision
  adapter (the visible original PalmNear/PalmFar meshes are unchanged).
- Author: DRG Software Solutions LLC; native contact adaptation by NeonCity.
- Source Website: official Tidewater GitHub repository.
- Direct Source URL: https://github.com/dgreenheck/tidewater/blob/4811ba48d795197de5621985f404e765c0b7c0ef/src/world/vegetation/PlantGeometry.js
  and the adjacent `VegNodes.js` at the same locked revision.
- License / Version / Date: MIT, copyright 2026; full notice retained.
- Attribution Requirement: retain the copyright and MIT permission notice in
  `Docs/ThirdParty/Tidewater-LICENSE.txt`, already staged for distribution.
- Download Date: no new download; uses the reviewed 2026-09-26 source intake.
- Local Source Path: `ThirdPartyReference/Tidewater/src/world/vegetation/`;
  native derivative `Source/NeonCity/World/Foliage/NeonCityPalmContact.h`.
- UE Destination Path: existing native vegetation component's transient nearby
  palm collision pool; no additional art package or per-tree Actor.
- Modified / Unmodified: source rest centerline/radius adapted to ten bounded
  capsule segments. Fine asymmetric root lumps and wind flex are approximations.
- NeonCity Usage: physically blocking curved trunks, keeping leaf crowns and
  the empty side of the curve traversable, with existing Chaos contact feedback.
- Decision: **FOUND ONLINE AND MODIFIED**; collision follows reviewed source
  geometry semantics, not a replacement visual model.
- Evidence / limits: `Docs/FoliageInteractionValidation.md`.

### Forest audio/coast runtime adapter — 2026-09-26

- Asset Name: Tidewater SoundScape/soundBank metadata and TerrainData coast field.
- Author: DRG Software Solutions LLC, copyright 2026; original recording authors
  remain individually listed in this manifest and the existing audio audit.
- Source Website: official Tidewater repository; Freesound source pages for audio.
- Direct Source URL: https://github.com/dgreenheck/tidewater/blob/4811ba48d795197de5621985f404e765c0b7c0ef/src/audio/SoundScape.js
  and adjacent soundBank.js; world/TerrainData.js and util/Noise.js at the same lock.
- License / Version / Date: MIT (2026) for code/metadata; the existing 43 recordings
  retain their individually verified CC0 1.0 Universal licenses.
- Attribution Requirement: retain Tidewater copyright/MIT notice; audio credit
  optional, all original authors retained in existing per-recording entries.
- Download Date: no new download; uses the reviewed 2026-09-26 intake.
- Local Source Path: `ArtSource/VerticalSlice/Tidewater/Audio/` and
  `Source/NeonCity/World/Audio/`; source receipts/oracles preserve exact hashes.
- UE Destination Path: `/Game/NeonCity/Art/Tidewater/Audio/DA_NCTW_Soundscape`;
  the same 144 previously imported SoundWaves are referenced without edits.
- Modified / Unmodified: original mixing/coast equations adapted to native C++;
  UE voice routing and grounded third-person stride adapter, no new samples.
- NeonCity Usage: local island wind/leaves, insects, dawn chorus, spatial birds
  and ground-surface footsteps. Contact-specific rustles remain separate.
- Decision: **FOUND ONLINE AND MODIFIED**; direct licensed implementation port.
- Evidence / limits: `Docs/ForestAudioValidation.md`.

### Physical foliage brush contact — 2026-09-26

- Asset Name: The Rustle of a Bush (public high-quality MP3 preview).
- Author: Montacue.
- Source Website: Freesound.
- Direct Source URL: https://freesound.org/people/Montacue/sounds/178615/
- Download URL (published on that individual page):
  https://cdn.freesound.org/previews/178/178615_3280198-hq.mp3
- License / Version / Date: CC0 1.0 Universal; original published 2013-02-21;
  actual individual license block verified before intake on 2026-09-26.
- Attribution Requirement: none under CC0; voluntary author/source credit retained.
- Download Date: 2026-09-26; precise UTC time in provenance.json.
- Local Source Path: `ArtSource/External/Freesound/BushContact/`;
  primary page snapshot `Docs/ThirdParty/FoliageAudio/178615.html`.
- UE Destination Path: `/Game/NeonCity/World/Foliage/Audio/SW_NC_FoliageBrush`.
- Modified / Unmodified: public preview retained unchanged; prepared 1.11458 s
  mono PCM16 derivative with DC removal, 12 ms edge fades and -9 dBFS peak.
  This is a lossy source preview, not falsely described as the original WAV.
- NeonCity Usage: short actual movement through soft plants, independent of
  the reviewed Tidewater wind/insect/bird ambience.
- Decision: **FOUND ONLINE AND MODIFIED**.
  The source describes shaken bamboo / movement through a bush, appropriate to
  contact foley. Original WAV download requires login and was not requested.
  No account, paywall bypass, unknown executable or third-party script is used.
- Alternative: Fupicat's Game Rustle Leaves is also CC0 but explicitly synthesized
  with Beepbox; **BLOCKED BY QUALITY** for the requested natural contact treatment.
  https://freesound.org/people/Fupicat/sounds/538152/
- Evidence: source and page hashes in provenance.json; derived-wave hash in
  `ArtSource/VerticalSlice/FoliageAudio/generated.json`. Native contact/output passes FoliageContactAudioNative01; perceptual acceptance
  remains open. See Docs/FoliageContactAudioValidation.md.

### Tidewater foliage native attribute adapter (2026-09-26)

- Asset Name / Author / Source / License: the same eleven canonical vegetation
  meshes credited above, DRG Software Solutions LLC, locked Tidewater repository,
  MIT; existing notice and source provenance retained. No additional download.
- Local Source Path: ArtSource/VerticalSlice/Tidewater/Vegetation/; original
  Geometry.json.gz stays unchanged; editable Blend/FBX derivatives repack data.
- UE Destination Path: /Game/NeonCity/Art/Tidewater/Vegetation/SM_NCTW_* and the
  two existing Canopy/Plant materials.
- Modified / Unmodified: four-UV + exact byte type/kind tags for UE Nanite;
  cosmetic material phase uses 16-bit BA. Geometry/wind floats retain precision.
- NeonCity Usage: dense hill vegetation with existing local wood collision,
  soft-contact bend and wind, supporting native instanced lighting/shadows.
- Decision: FOUND ONLINE AND MODIFIED. No new primitive replacement geometry.
- Evidence: source-derived AttributeContract.json and NativeAttributeLayout test
  verify 34,152 native corners; ForestRenderingValidation.md records failures,
  successful import, visual comparison and unresolved daylight balance.

The foliage adapter's direct source URL is
https://github.com/dgreenheck/tidewater/tree/4811ba48d795197de5621985f404e765c0b7c0ef .
Source website: GitHub; author: DRG Software Solutions LLC; license: MIT,
copyright 2026; attribution: retain copyright and full MIT notice. The underlying
source was acquired 2026-09-26; the adapter uses that same local intake.


## Tidewater original ocean compute programs (2026-09-27)

- Asset Name: Four-cascade OceanFFT spectrum, inverse FFT, persistent foam and mip programs.
- Author: DRG Software Solutions LLC.
- Source Website: GitHub / dgreenheck/tidewater.
- Direct Source URL: https://github.com/dgreenheck/tidewater/blob/4811ba48d795197de5621985f404e765c0b7c0ef/src/ocean/OceanFFT.js
- License: MIT, copyright 2026; exact locked notice retained.
- Attribution Requirement: retain copyright and full MIT notice with copies/derivatives.
- Download Date: original locked code intake 2026-09-26; no new binary art download.
- Local Source Path: ThirdPartyReference/Tidewater/src/ocean/OceanFFT.js;
  immutable capture ArtSource/VerticalSlice/Tidewater/Ocean/.
- UE Destination Path: Shaders/Private/Tidewater/Ocean/ and Source/NeonCityOcean/; world/material integration pending.
- Modified / Unmodified: original WGSL bodies captured unchanged; mechanical HLSL
  resource/syntax conversion, preserving source spectrum and FFT equations.
- NeonCity Usage: preparation for one shared ocean field used by rendering and
  water queries. Not yet substituted into the active game water surface.
- Decision: FOUND ONLINE AND MODIFIED. Nine strict standalone shader compilation
  gates pass; 243-field original/native GPU and independent NumPy checks pass. Visual/gameplay integration remains open.
- Evidence: Source.json, generated.json and Docs/OceanFFTValidation.md. Original
  code license does not grant rights to any unreviewed third-party binary asset.


## Tidewater shared water queries (2026-09-27)

- Asset Name: WaterQuery Eulerian surface solver and WaterSurface cascade attenuation.
- Author: DRG Software Solutions LLC.
- Source Website: GitHub / dgreenheck/tidewater.
- Direct Source URL: https://github.com/dgreenheck/tidewater/blob/4811ba48d795197de5621985f404e765c0b7c0ef/src/ocean/WaterQuery.js
- License: MIT, copyright 2026; retained exact locked notice.
- Attribution Requirement: retain copyright and full MIT notice.
- Download Date: locked code intake 2026-09-26; local source capture 2026-09-27.
- Local Source Path: ThirdPartyReference/Tidewater/src/ocean/WaterQuery.js and
  WaterSurface.js; immutable capture ArtSource/VerticalSlice/Tidewater/WaterQuery/.
- UE Destination Path: Shaders/Private/Tidewater/Ocean/WaterQuery.usf and
  Source/NeonCityOcean/NeonCityOceanQuery (Public/Private headers and implementation).
- Modified / Unmodified: original WGSL captured unchanged; HLSL/native resource
  adaptation. UE permits three independently tagged in-flight CPU copies.
- NeonCity Usage: bounded shared displacement queries; world/coast integration pending.
- Decision: FOUND ONLINE AND MODIFIED. Real original/native GPU comparison passes
 704 points. This currently captures only the original optional-no-terrain branch.
- Evidence: Source.json, generated.json and Docs/WaterQueryValidation.md.


## Tidewater ocean surface and original foam (2026-09-27)

- Asset Name: CDLOD ocean patch grid/morph, WaterSurface functions and original foam pattern.
- Author: DRG Software Solutions LLC.
- Source Website: GitHub / dgreenheck/tidewater.
- Direct Source URLs: https://github.com/dgreenheck/tidewater/blob/4811ba48d795197de5621985f404e765c0b7c0ef/src/core/CDLOD.js ; https://github.com/dgreenheck/tidewater/blob/4811ba48d795197de5621985f404e765c0b7c0ef/src/ocean/WaterSurface.js ; https://github.com/dgreenheck/tidewater/blob/4811ba48d795197de5621985f404e765c0b7c0ef/src/ocean/FoamTexture.js ; https://github.com/dgreenheck/tidewater/blob/4811ba48d795197de5621985f404e765c0b7c0ef/src/engine/gpu/Mipmaps.js
- License: MIT, copyright 2026; exact source notice retained in both intakes.
- Attribution Requirement: retain copyright and full MIT notice.
- Download Date: existing locked source checkout acquired 2026-09-26; local conversion 2026-09-27.
- Local Source Path: ThirdPartyReference/Tidewater/src/; immutable captures ArtSource/VerticalSlice/Tidewater/OceanSurface/ and OceanFoam/.
- UE Destination Path: Source/NeonCityOcean/; Shaders/Private/Tidewater/Ocean/; /Game/NeonCity/Art/Tidewater/Ocean/T_NCTW_OceanFoam.
- Modified / Unmodified: original CPU geometry/selection and generated WGSL captured unchanged; C++/HLSL resource/syntax adapter. Original foam WGSL and original mip render shader executed unchanged; all 11 RGBA16F mip levels packaged losslessly as standard DDS.
- NeonCity Usage: original sea geometry, foam density/bubbles/mottling/streaks. Complete native surface draw and coast integration remain pending.
- Decision: FOUND ONLINE AND MODIFIED. No replacement noise texture or third-party unreviewed binary asset.
- Evidence: Source.json, generated.json, BakeReport.json and Docs/OceanSurfaceValidation.md.

## PF-015 modern apartment furniture intake — 2026-09-27

The following individual official pages and the publisher's
[asset licence](https://polyhaven.com/license) were checked **before download**.
All three publish CC0 1.0 Universal: commercial use/modification permitted,
attribution not required. Voluntary source/author credit is retained here.
This is evaluation intake, not a furnished-interior or visual acceptance claim.

| Asset name | Author | Direct source URL | Local source path | Intended UE destination / usage |
|---|---|---|---|---|
| Modern Arm Chair 01 | Vibrant Nordic | https://polyhaven.com/a/modern_arm_chair_01 | `ArtSource/External/PolyHaven/modern_arm_chair_01/` | `/Game/NeonCity/Art/Interiors/Apartment/`; living-zone seating candidate |
| Modern Coffee Table 01 | Amin | https://polyhaven.com/a/modern_coffee_table_01 | `ArtSource/External/PolyHaven/modern_coffee_table_01/` | `/Game/NeonCity/Art/Interiors/Apartment/`; living-zone table candidate |
| Modern Wooden Cabinet | Patrik Pangerl | https://polyhaven.com/a/modern_wooden_cabinet | `ArtSource/External/PolyHaven/modern_wooden_cabinet/` | `/Game/NeonCity/Art/Interiors/Apartment/`; storage candidate |

- Source website: Poly Haven, distinct application user-agent for its official API.
- License version/date: CC0 1.0 Universal; verified 2026-09-27. Original publication
  dates and official metadata retained in each intake folder.
- Download date: 2026-09-27, completed after the pre-download review; actual UTC
  acquisition times and file hashes are recorded in each `provenance.json`.
- Modified/unmodified: downloaded source stays unmodified. No UE import yet.
- Safety: bounded official-host FBX and 2K PNG only; API size/MD5 plus local SHA256,
  file signatures and dimensions checked. No downloaded executable or script.
- Quality decision: modern wood/stone/leather forms fit the planned material
  family sufficiently for inspection; scale, branding, roughness, LOD, collision
  and native visual acceptance remain required before use.
- Final USED / MODIFIED decision remains pending; these source files are not
  automatically adopted into the playable room.
- CPU-only Blender audit confirmed source dimensions, triangles, UVs and material
  slots (`Saved/VerticalSlice/PF015InteriorMeshAudit01/Meshes.json`). No source was
  modified. Native visuals, LOD and collision still await integration review.


## Immersion ocean world adapter — 2026-09-27

- Asset name: Tidewater original ocean spectrum, CDLOD geometry, surface functions,
  foam and water queries, bound to the playable NeonCity coastal world.
- Author: DRG Software Solutions LLC.
- Source website: GitHub / dgreenheck/tidewater.
- Direct source URL: https://github.com/dgreenheck/tidewater/tree/4811ba48d795197de5621985f404e765c0b7c0ef/src/ocean
- Licence: MIT, copyright 2026. Full retained notice:
  `Docs/ThirdParty/Tidewater-LICENSE.txt`; shipped alongside binaries.
- Attribution: retain copyright and permission notice; no new asset download.
- Download date: locked checkout acquired 2026-09-26; runtime adaptation 2026-09-27.
- Local sources: `ThirdPartyReference/Tidewater/src/ocean/`, `src/core/CDLOD.js`,
  `src/player/BoatController.js`; immutable generated/audited source in
  `ArtSource/VerticalSlice/Tidewater/Ocean*` and `Shaders/Private/Tidewater/Ocean/`.
- UE destination: `Source/NeonCity/Water/`, `Source/NeonCityOcean/`,
  `/Game/NeonCity/Art/Immersion/Ocean/`; existing original foam texture retained.
- Modified: resource/coordinate/lifetime adapters, UE SingleLayerWater lighting,
  bounded physical sample leases. Original spectrum profiles and surface function
  bodies are reused. The source WebGPU renderer is not being claimed identical
  to Unreal's lighting. Original shore displacement/refraction and physical
  queries are now included; ShoreSim, overhanging breakers and WakeSim remain
  separate gates. New captures are in `ArtSource/Immersion/Shore/`.
- NeonCity usage: normal South Beach coastal renderer and vessel buoyancy.
- Decision: FOUND ONLINE AND MODIFIED; native gameplay/render validation pending.
- Provenance: `ArtSource/Immersion/TidewaterWorldMaterial.json`, guarded native
  import ledger and generated source hashes. User's latest instruction explicitly
  restores direct porting; the attachment's reference-only clause is superseded.


## Immersion handheld light — pre-download review 2026-09-27

- Asset name: Portable Searchlight; author: Elijah Cragg.
- Source website: Poly Haven; direct source: https://polyhaven.com/a/portable_searchlight
- Licence: CC0 1.0 Universal, verified on the asset page and https://polyhaven.com/license
  before downloading. Commercial use and modification permitted; attribution optional.
- Download date: 2026-09-27; acquisition time and file hashes are in provenance.json.
- Local source path: `ArtSource/External/PolyHaven/portable_searchlight/Handheld2k/`.
- Intended UE path: `/Game/NeonCity/Art/Immersion/Flashlight/`.
- Modified/unmodified: raw source remains unmodified. The Blender derivative
  removes eight disconnected raised letters, uses a hand-centred pivot and a
  +X optical axis. The material recipe masks the printed brand panel and replaces
  baked side-lettering color, normal and ARM with neighboring clean housing
  texels, then applies a coastal graphite finish. Raw maps remain unchanged.
  Three LODs and an optical socket are prepared for native UE authoring.
- NeonCity usage: PF-019 visible handheld searchlight, night exploration.
- Decision: FOUND ONLINE AND MODIFIED. The 17,406-triangle derivative is in
  `ArtSource/Immersion/Flashlight/`; native import and grip/beam inspection passed
  in ImmersionFlashlightGrip01. Continuous player acceptance remains open.
- Safety: official API, bounded FBX/2K PNG, size/MD5/SHA256 and signature checks;
  no source scripts or executables.
- Visual review: `Saved/VerticalSlice/ImmersionFlashlightReview01/` exposed baked
  lettering after mesh-letter removal. `ImmersionFlashlightReview02/` verifies
  the corrected `ArtSource/Immersion/Flashlight/Finish.json` treatment in Blender.
  Both runs remain retained; neither is native UE or in-hand evidence.

## Immersion handheld carry animation — 2026-09-27

- Asset name: Idle_Lantern_Loop, Universal Animation Library 2 Standard.
- Author: Quaternius / Gonzalo Furnier.
- Source website / direct URL: https://quaternius.itch.io/universal-animation-library-2
- Licence: CC0 1.0 Universal, previously verified 2026-09-25 before acquisition;
  retained official source and licence evidence in the library's provenance folder.
- Attribution: optional; voluntary author credit retained here.
- Download date: existing reviewed intake 2026-09-25; no new download.
- Local source: `ArtSource/External/Quaternius/UniversalAnimationLibrary2Standard/`.
- Modified: retargeted to the existing mannequin skeleton; only the right arm is
  blended into the player wrapper. No source animation or template graph replaced.
- Derivative: `ArtSource/Immersion/Flashlight/Animation/`, selected by AnimationRecipe.json.
- UE destination: `/Game/NeonCity/Art/Immersion/Flashlight/AN_NC_Handheld_Idle_Lantern_Loop`.
- NeonCity usage: visible flashlight grip without the pistol's extended index finger.
- Decision: FOUND ONLINE AND MODIFIED; native pose/grip review passed in
  ImmersionFlashlightGrip01 with a fixed palm offset; manual acceptance pending.

## Immersion ceiling fixture — pre-download review 2026-09-27

- Name: Modern Ceiling Lamp 01; author: James Ray Cock.
- Source: Poly Haven, https://polyhaven.com/a/modern_ceiling_lamp_01
- Licence: CC0 1.0 Universal, verified on that individual page and
  https://polyhaven.com/license before download; commercial use/modification
  permitted, attribution optional.
- Download date: 2026-09-27; exact time and file digests go in provenance.json.
- Local source: `ArtSource/External/PolyHaven/modern_ceiling_lamp_01/`.
- Intended UE path: `/Game/NeonCity/Art/Interiors/Apartment/`.
- Intended use: a visible ceiling fixture for the existing apartment light;
  preserve light budgets and unify finish with the reviewed furniture.
- State: licence cleared for bounded FBX/2K PNG intake; no adoption or native
  visual acceptance yet. Raw source stays unchanged, derivative tracked separately.

## Immersion apartment adoption — 2026-09-27

The four individually credited sources above (Modern Arm Chair 01, Modern Coffee
Table 01, Modern Wooden Cabinet, Modern Ceiling Lamp 01) are now **FOUND ONLINE
AND MODIFIED**. CC0 1.0, optional attribution and original acquisition dates
remain as recorded. Raw files remain unmodified. Derivatives have normalized
scale/pivots, preserved UVs, a coordinated finish, three LODs and appropriate
simple/cosmetic collision. Native import completed in Authoring-20260927-190141.

Actual UE destination: `/Game/NeonCity/Art/Interiors/Apartment/`, 4 meshes and
22 material/texture packages. Sources/derivatives are linked by SHA256 in
`ArtSource/VerticalSlice/imported.json` and the Interiors/Ceiling generated
receipts. NeonCity usage: two chairs, a living table, rear-wall storage and the
visible apartment ceiling light. Native placement completed in
Authoring-20260927-190329, recorded in `ArtSource/Immersion/Interiors/ApartmentLayout.json`.

Native material graph audit confirms source diffuse/normal/ARM connections and
no material compile errors. Base-color capture shows the intended wood/stone/
leather palette; final lighting review and complete room design remain open.
Adoption is not visual acceptance. No source branding or unfamiliar script is
introduced, no extra dynamic light is added, and the prior desk pieces remain
hidden for reversible restoration.

## Interior completion intake — not adopted (2026-09-27)

Electric Stove by Kuutti Siitonen, Poly Haven:
https://polyhaven.com/a/electric_stove. CC0 1.0 Universal, checked against the
individual asset and https://polyhaven.com/license before 2026-09-27 download.
Commercial use/modification permitted, attribution optional. Raw 2K FBX/PNG
files and verified hashes are retained at ArtSource/External/PolyHaven/electric_stove/.
UE destination: none; modified: no. Intended use: compact home/village kitchen.
Geometry audit preserves 13,914 triangles, UVs and source proportions. Visual
review found strong wear and a rear label in the diffuse atlas; cleanup and
native review are required before any adoption. No game asset changed.

Modern Bed by DClayman, OpenGameArt:
https://opengameart.org/content/modern-bed. Individual listing links CC0 1.0,
published 2015-09-05; checked before 2026-09-27 download, attribution optional.
Archive SHA256 ca6780c364322f49a3adab29e42ee30c37334968b394629a68120a6327b107a8,
137,201,911 bytes. Read-only archive inventory found only MAX geometry, JPGs and
PSD source files. Not adopted: unsupported authoring format. Raw archive retained
at Saved/VerticalSlice/InteriorCandidateIntake/ModernBed/, outside shipping assets
and Git. UE destination: none; modified: no; extraction/execution: none.


## Home bedroom bed — reviewed derivative, native adoption pending

- Asset: Home bedroom (only the bed, mattress, duvet and five pillows).
- Author: DREAM_SEARCH_REPEAT. Source: OpenGameArt.
- Direct source: https://opengameart.org/content/home-bedroom
- Download: https://opengameart.org/sites/default/files/home_bedroom_0.blend
- License: CC0 1.0 Universal, published 2025-05-06; checked before download
  on 2026-09-27. Commercial use and modification permitted; attribution optional.
- Raw source: ArtSource/External/OpenGameArt/HomeBedroom/; byte-identical
  source SHA256 e4c8bb1a77fa827116cb4030199d167f5a1519f45390f3c1bfa159875609f685.
- Derivative: ArtSource/Immersion/Bed/; bed-only selection, grounded pivot,
  2.25 x 1.50 x 1.00 m, 6,372 triangles, softened frame and cloth contours,
  coordinated finish and a 512-square linen normal for mip streaming.
  Raw source is unmodified; derivative is modified. Source scripts disabled.
- Intended UE destination: /Game/NeonCity/Art/Interiors/Apartment/SM_NC_Interior_Bed.
- NeonCity usage: the existing starter apartment bedroom area. No unrelated
  room, painting or decoration is included.
- Decision: FOUND ONLINE AND MODIFIED, prepared and inspected from both sides
  in Blender. Native placement, collision and visual verification pending.

Native bed import completed in Authoring-20260927-201724; placement completed
in Authoring-20260927-201859. The stated UE destination now exists: one mesh,
six materials and five textures. Three LODs, 26-DOP collision, 150 x 225 cm
footprint. Four capsule routes pass. Bed base/mattress/pillow prototype actors
are retained hidden with no collision. The initial extra inspection route
intersected existing room furniture and aborted before any map save; the
valid foot-of-bed route is recorded in ArtSource/Immersion/Bed/ApartmentLayout.json.
No light, property identity, spawn or garage state was changed. Native visual
review and whole-room design remain open.

## Phase 20.5c — 13-upgrade candidate provenance (2026-10-08)

These are recoverable candidates. Native maps and newly imported packages remain
in Saved/IslandLandPass1/Automation15/Workspace until objective acceptance and
explicit adoption. A prepared or imported asset is not a visual PASS.

- **Nacre Spyder exterior and roof:** NeonCity original coachwork, wheels,
  retracting panels, tonneau and hinge/carriage design. Sources and derivatives:
  ArtSource/Phase205cUpgrade/Nacre/; native candidate destination:
  /Game/NeonCity/Art/Phase205cUpgrade/Nacre/. The exterior is not a renamed
  licensed Ferrari body. Only selected seat/dashboard trim and tyre details
  reuse the existing Ferrari 458 donor by **vicent091036**, **CC BY 4.0**:
  https://sketchfab.com/3d-models/ferrari-458-italia-57bf6cc56931426e87494f554df1dab6.
  Original donor attribution and licence evidence remain in the earlier entry
  and source intake. Raw GLB SHA256:
  cafe3f48da6797aa9bde75ca768bc5b57db366575fd233e90df186ae988a876e.
  Earlier verification used the upstream model-licence audit because the
  original Sketchfab listing was unavailable; this is retained explicitly
  in generated.json. Attribution is required
  in distribution credits. Donor parts were selected, refitted and recoloured;
  the immutable raw donor and its original licence evidence are retained.
- **Nacre engine audio:** “Big Block V8 Rev 2.mp3” by **Henaway**, **CC0 1.0**,
  https://freesound.org/people/Henaway/sounds/127981/. Raw source SHA256:
  4bd3d63c879f5beffb9af822c7c3752c0746ce19df242d465aa4b00b2982e9a1.
  ArtSource/Phase205cUpgrade/Nacre/Audio/generated.json records source,
  derivative hashes and eight layers. Modified by spectral/timbre extraction,
  exhaust grains and original flat-plane V8, shift, spindle and latch synthesis.
  Attribution optional. Audio exists and drives native RPM/roof events; subjective
  sound acceptance remains USER AUDIO REVIEW REQUIRED.
- **Ridge residence, roof pad, lift, BBQ apparatus and tank:** NeonCity original
  authored geometry. Editable recipes and native receipts are under
  Tools/Art/Phase205cUpgrade/, ArtSource/Phase205cUpgrade/ and the QA workspace's
  Saved/Phase205cUpgrade/. Existing licensed furniture, fish, textures and
  animations retain their earlier individual provenance. No reference
  architect's drawings, photographs or meshes were copied into these assets.
  Source study: Docs/Phase205cUpgrade/ArchitectureReferences.md distinguishes
  completed buildings from proposals. Geometry and functional imports do not
  yet establish whole-house luxury visual acceptance.
- **Local summit road, earthwork and geology:** NeonCity original road and
  structural work. ArtSource/Phase205cUpgrade/summit-terrain.json records the
  native baseline, scoped edit mask and exact candidate hashes. SummitGeology/
  preserves original UV/topology/materials and refits only the two affected
  existing geological overlays. FoundationClearance/ preserves all other
  completed concrete vertices, relocating two road-intruding piers to surveyed
  rock with full visible-triangle collision. Original source files are retained.
- **Stern wake source adaptation:** **Tidewater**, **DRG Software Solutions LLC**,
  **MIT**, https://github.com/dgreenheck/tidewater; source commit
  4811ba48d795197de5621985f404e765c0b7c0ef. Source checkout:
  ThirdPartyReference/Tidewater; licence and credits:
  Docs/ThirdParty/Tidewater-LICENSE.txt and Tidewater-CREDITS.md. Actual adapted
  source files are src/ocean/WakeSim.js, WaterSurface.js and FoamTexture.js.
  The foam texture is generated by source code; no fabricated external
  wake.png is attributed. UE modifications suppress vessel bow/side injection,
  retaining water-fixed stern history, natural waves/shore foam and hull forces.
  Preserve the MIT notice. Source third-party asset credits are separate from
  this code licence. Original source GPU execution retains1560frames and21
  real shader snapshots using declared analytic hull/terrain/sky fixtures.
  Native material inline shader actually compiled/saved55; complete native
  seven-stage input run59 PASS,21 snapshots and842 continuous original frames
  packaged at recorded game timestamps. Bow/sides clean, stern history and
  stop decay visible. Source/native speeds and lighting differ; source's
  already-stopped snapshot is not a matched deceleration reference. Day top
  glints obscure parts, and width/transparency differ. Final current performance
  and root adoption remain. Provenance and review paths:
  Docs/Phase205cUpgrade/TidewaterWakeProvenance.json.

Pet searches have not produced an accepted shipping asset. No licence, model
quality or completed pet behaviour is claimed for a search result.

### Phase20.5c retained chair rest and local road rock repair (2026-10-08)

- Two existing `SM_NC_Interior_Armchair` instances retain their credited source,
  materials, visible mesh, placement and BlockAll collision. Real visible seated
  occupants reuse `AN_NC205C_Sitting_Idle_Loop`, derived from Quaternius / Gonzalo
  Furnier, Universal Animation Library Standard, CC0-1.0. Original source and
  verified licence: `ArtSource/External/Quaternius/UniversalAnimationLibraryStandard/`;
  [author source](https://quaternius.itch.io/universal-animation-library). No
  attribution required. Existing retarget recipe/source skeleton retained.
  NormalF/shared timed recovery/safeF6/F7 and repeatedWstand74 PASS; genuine
  independent-process76 PASS. Actual close chair/body/feet PNG reviewed without
  obvious penetration. These seats do not implement bed sleep or other facilities.
- Local rock derivatives: original NeonCity adaptation of the previously
  credited summit geology source; existing material licences/credits retained.
  Editable source `ArtSource/Phase205cUpgrade/HikeRockClearance02/` preserves every
  height and high-cliff vertex, but native station706 remained obstructed. The
  visible705..707 notch03 passed39road height probes but real walking hit the
  next between-sample face at708. Notch04 cuts only the bounded704..716 road
  envelope from1973 original triangles, interpolating existingUVs; every original
  vertex coordinate stays exact. Exported visible geometry is also full physical
  collision; no invisible collision deletion. Native import82 PASS; real walk/
  changed local visual acceptance pending. Wide64m height lowering01 rejected
  and retained for audit; it is not the native QA map surface.

### Phase20.5c close cooking and scoped cut/fill candidates (2026-10-08)

- BBQ contact is original NeonCity procedural presentation on the retained real
  player skeleton: finite25-degree torso lean, normal collision-swept station
  footsteps, exact31cm clamp-to-handle spacing and shoulder-directed tongs.
  Root player animation packages remain protected; one additive spine node was
  compiled/saved in the QA copy before the existing finite-length hand IK.
  Native94 genuine partial-cook restore/resume/flip/plating/Store/leave PASS;
  actual palms/tool/shelf close evidence reviewed, maximum wrist errors1.596cm
  right/0.682cm left.131 original frames form13.75s continuous native evidence.
  Earlier full transactions remain retained; current complete sequence and
  ordinary fishing regression are pending.
- Native84 proved only local walking/collision after notch04. Its actual road
  close images still showed stretched cliffs and needle faces; full hike86
  failed before the notch at703. These are retained objective failures.
  `ArtSource/Phase205cUpgrade/HikeRoadBatter02/` is a new original bounded road
  cut/fill candidate, regenerated against the exact native baseline export
  after source01 was refused.1848 height samples change in a51x81 envelope;
  all outside samples are exact. Source01/02 and before-map copies are retained.
  Its continuous original dense relief mesh replaces the rejected triangle-hole
  approach and refits only the bounded region. PBR uses the earlier individually
  credited CC0 Poly Haven rock_face_03, forest_ground_04 and gravel_ground_01
  textures with original metric world projection; no scan-mesh claim is made.
  Native import, physical clearance and visible cut-slope quality remain pending.
- KayKit Character Animations1.2, author Kay Lousberg, CC0-1.0, no attribution
  required. [Author page](https://opengameart.org/content/kaykit-character-animations).
  Local source: `ArtSource/External/KayKit/CharacterAnimations12/`; unchanged ZIP
  SHA256c9d3fbea492dc6edd0903939369a564c2240b892430bcd99e0aee4876110bb8f,
  licence and every extracted file hash in provenance.json. Read-only inspection
  found its LayingDownIdle on a six-bone rigid prototype rig with no articulated
  legs/elbows. It is an inspected source candidate, not an accepted or retargeted
  realistic player sleeping animation. No shipping animation quality is claimed.

### Phase20.5c sleeping and native tour/fleet checkpoints (2026-10-08)

- The existing bed is DREAM_SEARCH_REPEAT's CC0 Home bedroom asset, retained
  unchanged in `/Game/NeonCity/Art/Interiors/Apartment/SM_NC_Interior_Bed`.
  [Author source](https://opengameart.org/content/home-bedroom). Its mesh,
  material and collision are reused. Original NeonCity articulated supine
  breathing is `ArtSource/Phase205cUpgrade/HomeBedMotion02/`; its FBX SHA256 is
  `1bf30ee24d51d2cbb2315f289e8c1d829a8034da5b5b11cd313bf7e38be3304b`.
  Original standing/turn/sit entry and reverse exit refine only the unfinished
  approach, in `ArtSource/Phase205cUpgrade/HomeBedApproach03/`: 115 keys each,
  3.8 seconds, no root motion. Enter SHA256
  `236892b57b98c13b93c3aac38251912d54d4a2bfb6f02089f399bfd85eb01337`;
  exit `3e56b7cb380ded1f27abadf75c3e45cf6f7f7810fea578e616c40965981e8939`.
  All clips use the retained Epic reference rig; no external rigid prototype
  was retargeted. Root skeleton and original sleeping package remain exact.
  Native146 moved only the bed/rug/pendant/rest anchor/real lamp group 400cm
  east for a clear left-side approach. Original129 six-hour/time/rest/save
  assertions passed before its cancellation-driver failure; changed148 actual
  mapped approach/entry/body/cancel/F7/W exit passed. Subjective motion and
  whole-room luxury remain open; this is a QA candidate, not main-map adoption.
- Native139 creates one bounded master-suite exposure volume with EV100 floor3,
  maximum15.8 and bias0. Existing actual2800lm lamps are unchanged. Native148
  normal-rendered body and six room/window views are retained. Empty private
  circulation and clipped Sunset window highlights remain objective defects;
  they have not been accepted as luxury lighting.
- Shoulder119's 36 signed-side height samples and matching304 visible rock
  vertices retain the entire road/walking bed and unrelated terrain exactly.
  Exterior133 uses surveyed outside cameras, proving the old giant-fan view
  was inside the cliff. Smaller genuine backface/fold artifacts remain. Read-only
  source153/154 identifies actual corresponding polygons. Horizontal micro-fold
  candidates01/02 fail their own improvement guards and were never exported or
  imported. Their failure is retained; no performance-driven geometry reduction,
  new treeLOD, hidden collision deletion or full-island regeneration is used.

### Phase20.5c residence facilities and companion candidate (2026-10-08)

- Wardrobe retains the imported CC0 dresser by **Patrik Pangerl / Poly Haven**.
  Original metric garage tracks, ten articulated leaves, controls and storage
  shelves are in `ArtSource/Phase205cUpgrade/GarageWardrobeParts01/`.
  `generated.json` records each FBX/source hash. Native189 saves these to the
  isolated QA map; source furniture and original two bay anchors are retained.
  Controls use the existing property and owned vehicle records.
- Cinema retains the existing licensed CC0 armchairs. Cycling body/pedals,
  finite enter/cycle/exit clips on the existing Epic reference skeleton, cedar
  sauna equipment/enclosure and lighting controls are **NeonCity originals** in
  `ArtSource/Phase205cUpgrade/LeisureFacilities/` and `Supplement01/`. The
  30-second procedural *Coastal Light* film and generated facility audio are
  original. `Audio/OriginalLeisurePartyLoop.wav` is an original16-second loop;
  source and hash are recorded in `Audio/generated.json`. Native190 saved
  facilities; whole190 failed its later party sound argument. Native193 saved
  party, companion and room lamps. Actual195 gym F/finite session/energy/F6/F7
  passed; this does not accept the whole leisure floor or architecture.
- The retained **Microsoft Rocketbox MIT / Tidewater MIT** male skin and body
  texture remain unchanged. A separate native owned textile material derivative
  duplicates the original normal/ORM/albedo graph and masks blue/teal fabric
  texels before preserving luminance with an owned palette. Its original head
  material is untouched. Native205 saves the derivative under
  `/Game/NeonCity/Art/Phase205cUpgrade/OwnedAppearance/`; actual normal-input
  tint, skin and equipment review remain required. No clothing asset is
  replaced with a vehicle paint material.
- **Benny, the Rottweiler**, by **umask007**, is from
  [OpenGameArt](https://opengameart.org/node/18287), **CC-BY-SA3.0**.
  Source: `ArtSource/External/OGA/Benny/benny.blend`, SHA256
  `f8078954ea192f8cee33f552e7510adf2468b5b5a85b8e17a06c60e05392863e`.
  Uniform scale0.45, material restoration and original idle/run exports are in
  `ArtSource/Phase205cUpgrade/BennyPet01/`; original geometry and rig are retained.
  The derivative and source are **CC-BY-SA3.0**, with notices and texture-source
  attribution in `ATTRIBUTION.txt`. Rottweiler.jpg is credited by its source to
  Dr.Manfred Herrmann / ADRK under CC-BY-SA3.0; its photographer is not explicitly
  identified. Rottweiler-Head.jpg is **Rains27 / Rainer Lemmerer**, public domain.
  The1000-triangle,1024-texture stylized asset and slowed source run are a
  candidate, **not accepted realistic hero pet quality or a true walk clip**.
  Native193 imports its own skeleton without altering the player skeleton.
  Ordinary CMC capsule and full-body probes drive movement; the display mesh
  intentionally has no physics collision/ragdoll. Native207 removes the
  unsaved autogenerated physics dependency. Follow/stay/home/cold and visual
  evidence are separate from import success.
- Rock16 preserves all triangles, texture UVs and original single-sided rock
  material. Native185 local walking/support passed; actual exterior frames
  retained small vertical slivers. Bounded refit20 corrects heights originally
  fitted before horizontal unfolding, using the current unmodified native
  grade. It changes27768 heights within17m/below250m, maximum14.1103m, and keeps
  originalXY/topology/UV, outside points, high cliff/house cuts, terrain, road,
  navigation and treeLOD. The same native mesh supplies visible and complex
  collision surfaces. Source17/18/19 guard failures are retained and were not
  imported. Native205 import is not a whole exterior visual PASS.

### Phase20.5c retained facility derivatives and timing correction (2026-10-08)

- **GarageClearance214** derives from the existing **NeonCity original**
  `Prepared/SM_RidgeHouseMetal.blend` and
  `GarageWardrobeParts01/SM_AureliaTracks.blend`. It corrects physical frame/track
  clearance around the existing ten-leaf mechanism; it does not introduce a new
  downloaded model or collision exclusion. Source and output hashes are in
  `ArtSource/Phase205cUpgrade/GarageClearance214/generated.json`.
  UE destination: `/Game/NeonCity/Art/Phase205cUpgrade/GarageClearance214/`.
  Native221 receipt `QA/Saved/Phase205cUpgrade/GarageClearance214b/receipt.json`
  preserves original packages, actor poses, visible complex collision and panel
  meshes. Real normal-F close/open/light controls222 passed; stored retrieval
  and occupied reversal are separate. The rejected219 attempt remains retained.
- **Screen218** is a material derivative of the original **NeonCity** procedural
  *Coastal Light* film in
  `ArtSource/Phase205cUpgrade/LeisureFacilities/CoastalLight.ush`.
  `RepairCinemaScreen218.py` duplicates `M_LeisureFilm` as
  `/Game/NeonCity/Art/Phase205cUpgrade/LeisureFacilities/M_LeisureFilm_Calibrated218`
  and adds a local12-emission multiplier. Original material, screen mesh,
  global exposure/Bloom and lights stay unchanged. Native221 receipt:
  `QA/Saved/Phase205cUpgrade/CinemaScreen218/receipt.json`. Actual222 normal-F
  playback and distinct6s/12s frames passed screen legibility. This remains an
  original procedural short; no purchased movie, high-end film content or whole
  cinema visual quality is claimed. There is no new Screen218 FBX source pack.
- **GymHands227** derives from the project's original three finite bike clips
  and the retained216 source candidate. It changes only arm/finger channels to
  compensate the existing `AnimationScaled` lengths and calibrate actual Wood
  skin contact. `ArtSource/Phase205cUpgrade/GymHands227/generated.json` records
  original/output hashes, unchanged body/feet, actual-rig FK and skin measurements.
  The existing Epic reference skeleton is retained. The previously credited
  **Microsoft Rocketbox MIT / Tidewater MIT** player mesh/texture serves only as
  a read-only skin reference; no new player geometry or texture is distributed.
  Original230/237 read-only exports and the prior native216 right-hand failure
  are retained. Native238 imported227, and241 actual hand03 proxies passed, but
  the whole finite test failed Active pedal phase: reference FBX import had
  changed the source clock from30 to25FPS. Visible palms/thumbs/sleeves were
  reviewed; under-grip skin is occluded. Import/contact results do not establish
  whole-body animation or premium room acceptance.
- **GymHands245** is solely a timing derivative of227. No IK, new pose, skin
  solver, character mesh or finger asset is produced. All original key values,
  frame indices and handles have identical hashes; every bone matrix at every
  source frame is unchanged. Setting export30FPS restores Enter2s/Cycle2.4s/Exit2s.
  Provenance and exact-key guards:
  `ArtSource/Phase205cUpgrade/GymHands245/manifest.json` and `generated.json`;
  generator: `Tools/Art/Phase205cUpgrade/RetimeGymHands245.py`.
  UE destination: `/Game/NeonCity/Animations/Phase205cUpgrade/GymHands245/`.
  Actual249 receipt `QA/Saved/Phase205cUpgrade/GymHands245b/receipt.json` verifies
  true native lengths2.0/2.400000095/2.0s, all three duration guards and unchanged
  old216/227 packages, root skeleton, actor poses and seat/equipment offsets.
  Only three QA bike references change. Failed247 Python API attempt and241 real
  F6 remain preserved. Corrected finite PIE/pedal/energy/save acceptance is pending.
- **InteriorFinish01** reuses19 existing native furniture/wall actors, without
  new downloads or mesh/material/collision/LOD edits. The verified source index
  is `ArtSource/Phase205cUpgrade/InteriorFinish01/provenance.json`:
  CC0 Modern Arm Chair01/widened sofa by **Vibrant Nordic**, Modern Coffee Table01
  by **Amin**, Modern Wooden Cabinet by **Patrik Pangerl**, and Potted Plant02 by
  **Rico Cilliers**. Original individual Poly Haven provenance and optional
  attribution remain as recorded above; Rug/CoastalArtwork/AcousticWall are
  retained NeonCity originals. Existing UE packages under `Art/Interiors/Apartment`,
  `Art/IslandLandPass1` and `Art/Phase205cUpgrade/LeisureFacilities` are reused.
  Native230 placement receipt and actual235 support/foot/camera receipt are in
  `QA/Saved/Phase205cUpgrade/InteriorFinish01/` and
  `QA/Saved/VisualOverhaul/Upgrade-VillageRescueInterior235/InteriorFinish/`.
  The21 original native images were reviewed separately: local dusk/night room
  and stair lighting gaps remain. Lighting250 failed its first Arrival structural
  ceiling trace before actor creation/save; native252 also failed with no save.
  Native257 guarded Lighting01c now verifies five actual downward structural
  ceilings and original emission sockets, reusing the existing CC0 pendant
  recorded in `ArtSource/Immersion/Ceiling/` at five affected rooms. It preserves
  existing mesh/material packages, old lights, poses, exposure and the production
  map. Receipt: `QA/Saved/Phase205cUpgrade/InteriorLighting01c/receipt.json`.
  Fifteen current affected-room Day/Sunset/Night renders remain a separate gate.
  Placement/capture completion does not certify premium interior quality.

### Additional dog search candidate — blocked, not acquired (2026-10-08)

- Listing candidate: *Animated Dog - Sits, Rolls Over, Shake Paw* by
  **LasquetiSpice**, [official Sketchfab listing](https://sketchfab.com/3d-models/animated-dog-sits-rolls-over-shake-paw-d9020159339145e6b9ecd5f3d830830f).
  Search metadata indicates approximately33.4k triangles and CC Attribution;
  the exact licence version has not been audited. Official page/API access
  returned403. No file, download hash, rig/action inspection, redistribution
  clearance, native import or visual quality acceptance exists. This blocked
  search does not promote the retained Benny prototype to realistic hero quality.

### Mainland Riviera914/915 — licensed reference adaptations (2026-10-11)

- Mainland SouthBeach expansion within Phase20.5c: four connected streets,
  sixteen individually assigned architectural references, a motor atelier and
  two placements of the existing Riviera35 yacht. The island map is preserved.
- Acquired Mediterranean Apartment Building by **Volunick Volunick** and
  Porsche911 by **AmJViZ**, under the documented Blendkit Royalty Free grant;
  original packs and exported FBXs remain internal. Geometry is adapted, source
  bitmaps with unclear independent provenance are replaced, and unknown vehicle
  badge/plate artwork is omitted. The911 model year is not established.
- **James Ray Cock / Poly Haven**, CC0 Modular Urban Apartments Facade, supplies
  the retained textured modules for eleven individual assemblies. Original
  cornices, porticos, curved corner, roof variants, shutter details, mosaics and
  sixteen floor/ceiling/party-wall shells supplement the licensed modules.
  **Charlotte Baglioni / Poly Haven**, CC0 Leafy Grass, supplies retained2K PBR
  ground maps. Existing yacht attribution remains **angelo raffaele catalano**,
  CC BY4.0; this pass does not introduce a second yacht design.
- [Source credits](ThirdParty/Mainland914-Credits.txt),
  [real reference assignments](Phase205cUpgrade/MainlandStreetReferences915.md)
  and [delivery and limitations](Phase205cUpgrade/MainlandDelivery914.md) are
  authoritative. Real architectural pages are references, not texture licences
  or evidence of surveyed one-to-one reproduction. Final native SHA256 records
  and actual runtime receipts are in
  `ArtSource/Phase205cUpgrade/Mainland914/NativeAdoption914.json`.
