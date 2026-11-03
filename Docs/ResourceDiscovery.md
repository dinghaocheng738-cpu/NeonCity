# Resource discovery and decisions

Reviewed 2026-09-25 using public official pages and author repositories. Do not
treat a search result or a site's word “free” as approval. Downloaded source
provenance and actual usage are recorded in ArtAssetManifest.md.

| Category / candidate | Evidence | Decision at this checkpoint |
|---|---|---|
| Road PBR: Asphalt 01 | [Poly Haven](https://polyhaven.com/a/asphalt_01), asset-level CC0 | **FOUND ONLINE AND MODIFIED**: world-scaled restrained finish; actual map integration |
| Beach PBR: Damp Beach Sand | [Poly Haven](https://polyhaven.com/a/damp_beach_sand), CC0 in actual asset page/structured data | **FOUND ONLINE AND MODIFIED**: fine grains, 2 m scale, dry/wet adaptation |
| Beach PBR: Coast Sand 05 | [Poly Haven](https://polyhaven.com/a/coast_sand_05), CC0 | **BLOCKED BY QUALITY** for South Beach: dark/gravelly colour and surface; no UE import |
| Street prop: Fire Hydrant | [Poly Haven](https://polyhaven.com/a/fire_hydrant), CC0 | **FOUND ONLINE AND MODIFIED**: reduced clean variant and coherent finish |
| Street lights | [Street Lamp 01](https://polyhaven.com/a/street_lamp_01), [02](https://polyhaven.com/a/street_lamp_02), CC0 | **BLOCKED BY QUALITY** for modern main street: ornate Victorian design. Not downloaded; existing original lamp remains pending further search |
| Animation | [Quaternius Standard](https://quaternius.itch.io/universal-animation-library), [publisher](https://quaternius.com/packs/universalanimationlibrary.html), ZIP licence | **FOUND ONLINE AND MODIFIED**: 43 actions; 11 retargeted; eight in shared character graphs plus Driving_Loop in the occupant graph. Real PIE captures verify poses; final fingers, long-gun poses and recovery polish remain open |
| Modular urban architecture | [Quaternius Downtown City MegaKit](https://quaternius.com/packs/downtowncitymegakit.html), [Kenney City Kit](https://kenney.nl/assets/city-kit-commercial), asset-level CC0 | Candidate review continues. Boston/NYC architecture and simplified low-poly buildings require coastal style adaptation; no package adopted yet |
| Generic cars | [Kenney Car Kit](https://kenney.nl/assets/car-kit), [Quaternius Cars](https://quaternius.com/packs/cars.html), CC0 | **BLOCKED BY QUALITY** as finished hero assets: simplified silhouettes/materials. Could support references; neither adopted yet; this is not a claim that all online cars are unsuitable |
| Generic Sedan, MMC Works | [Author Sketchfab page](https://sketchfab.com/3d-models/generic-sedan-car-58c33766470d46e7b2aed542650494e5) | **BLOCKED BY LICENSE / ACCESS REVIEW**: search index mentions CC-BY and NoAI; direct page returned 403. No download, no reuse, no assumption that these terms permit this workflow |
| Marine base | [Kenney Watercraft Kit](https://kenney.nl/assets/watercraft-kit), CC0 | **BLOCKED BY QUALITY** as finished yacht/speedboat: low-poly kit requires major hero redesign; no adoption yet |
| Aircraft base | [WWI Airplane, Kala1987](https://opengameart.org/content/ww1-wwi-airplane-model-obj), actual asset CC0 | **BLOCKED BY QUALITY** for the required contemporary light aircraft: historical type and limited detail; not downloaded |
| HDRI | [Poly Haven coast catalogue](https://polyhaven.com/hdris/coast-water), [licence](https://polyhaven.com/license) | Discovery completed; no individual HDRI adopted. Preserve dynamic day/night lighting while evaluating environments; no static sky substitution |
| PBR alternative | [ambientCG licence](https://docs.ambientcg.com/license/) | Publisher CC0 verified. No individual asset acquired; asset-level review is still required before intake |

Remaining search categories before their workstreams: modern street lighting,
coastal architecture modules, beach furniture/vegetation, generic hero vehicle
bases/interior details, helicopter/aircraft/submarine reference geometry,
fishing/diving props, animals, legal audio and radio. Do not fabricate final
decisions for categories not yet evaluated.

Further author-level checks on 2026-09-25:

- [Universal Animation Library 2](https://quaternius.com/packs/universalanimationlibrary2.html):
  official page and the free Standard archive agree on CC0 1.0. Its 43 actual
  actions include get-up, phone and carrying; they do not supply the full paid
  catalogue or the missing rifle set. Acquired for evaluation, no runtime import.
- [Kenney Blaster Kit](https://kenney.nl/assets/blaster-kit): CC0, but stylized
  science-fiction shapes are **BLOCKED BY QUALITY** for the current pistol.
- [Quaternius Ultimate Guns](https://quaternius.com/packs/ultimategun.html) and
  [Animated Guns](https://quaternius.com/packs/animatedguns.html): CC0 on author
  pages. Candidate libraries only; recognizable real-type silhouettes would
  require original redesign. No package downloaded.
- [Pistol by loafbrr_1](https://opengameart.org/content/pistol-5): the author page
  and downloaded README agree on CC0 after the documented historical correction.
  **FOUND ONLINE AND MODIFIED**: safe FBX/PNG intake, editable original silhouette
  and finish changes, measured hand contact and official UE import. Original
  source remains separate. See ReviewedPistolValidation.md for actual PIE evidence
  and the open final visual gate.

Further discovery on 2026-09-26, before any long-gun/audio adoption:

- [SMG-10 3D by GGBotNet](https://opengameart.org/content/smg-10-3d): asset page
  declares CC0; untextured 1,140-triangle source is a candidate needing silhouette,
  material and game-scale review. Not downloaded or used.
- [Benelli M1 by Jean PFP](https://opengameart.org/content/benelli-m1): page CC0,
  but its named real model is unsuitable for direct NeonCity hero use. A distinct
  original derivative would require further geometry review; no download/use.
- [Gun reload sounds by SpringySpringo](https://opengameart.org/content/gun-reload-sounds):
  author describes their own airsoft recordings, page CC0 1.0 with optional credit.
  Candidate for audible reload feedback; no audio download/adoption yet.
- [Boat motor.wav by pawsound](https://freesound.org/people/pawsound/sounds/154833/):
  actual author page links CC0 1.0, describes an original rough boat motor and
  offers a 48 kHz, six-channel recording. Original download requires login;
  no login/original download performed. The author's publicly linked HQ MP3
  preview was acquired after the same CC0 check, decoded into a mono loop and
  retained with its receipt. **FOUND ONLINE AND MODIFIED**: official UE import
  and runtime playback checks pass; audible quality review remains pending.
- [Steamboat Engine Sound by Spring Spring](https://opengameart.org/content/steamboat-engine-sound):
  actual asset page declares CC0 1.0. Its stated steam-engine character is
  **BLOCKED BY QUALITY** for a contemporary speedboat motor; no download/use.
- [Engine/Mechanic working sound by jwiese](https://opengameart.org/content/enginemechanic-working-sound):
  actual page offers CC0 alongside CC-BY 3.0, with start/loop/end FLAC files.
  This is a candidate requiring an audible quality check, not an adopted asset.
- [Particle Pack by Kenney](https://kenney.nl/assets/particle-pack): publisher
  page and downloaded archive notice both grant CC0 1.0. The archive identifies
  pack 1.1 and credits filter-template contributors, retained in provenance.
  Three original smoke masks were extracted and visually inspected. Smoke 01's
  finer irregular breakup is **FOUND ONLINE AND USED** as an unchanged mask in
  an original lit foam material; 04 is too soft/bulbous as a water surface.
  Official import and actual MarineThrottleLoop05 wake/HUD checks pass after
  rejecting Loop04's separated puffs. Full marine visual acceptance remains open.

## Connectivity and safe intake

Further aviation discovery on 2026-09-26:

- [Portable Searchlight by Elijah Cragg](https://polyhaven.com/a/portable_searchlight):
  actual asset page and publisher licence agree on CC0 1.0. Official API supplies
  a 506,380-byte FBX with verified size/MD5; source SHA-256 is retained. The actual
  local mesh review exposed lettering and a handheld battery shell. The derivative
  retains only the optical head, removes the handle/lettering/control shell and
  adds a distinct aircraft housing and NeonCity palette. **FOUND ONLINE AND
  MODIFIED**, with official UE intake and AircraftSearchlight08 night validation
  passed. Five actual HUD/world images were inspected; whole-aircraft appearance
  and final uninterrupted aviation acceptance remain open. No publisher render
  was downloaded or shipped; review renders are ours.
- [Security Light by Maximilian Schuster](https://polyhaven.com/a/security_light):
  CC0 on its actual page; a weathered industrial wall fixture, **BLOCKED BY QUALITY**
  for this compact gimballed aircraft fitting. No download/use.
- [Helicopter SFX by WuxiaScrub](https://opengameart.org/content/helicopter-sfx)
  and [Propeller (Cartoon) Loop by Mish7913](https://opengameart.org/content/propeller-cartoon-loop):
  author pages declare CC0. Their described distant pass/cartoon style is
  **BLOCKED BY QUALITY** for near-pilot continuous rotor audio. Not acquired/heard.
- [Helicopter Rotor Loop by qubodup](https://freesound.org/people/qubodup/sounds/187681/):
  CC0 declared, but its described government-video source is not identified in
  the rendered page. **BLOCKED BY LICENSE** under our provenance gate until the
  original is verified; no finding of infringement and no download/use.
- [Helicopter circling overhead by C-V](https://freesound.org/people/C-V/sounds/510114/):
  CC0, firsthand distant recording with birds and neighborhood ambience according
  to its author. **BLOCKED BY QUALITY** for a close continuous engine loop. No
  download or audible assessment. Rotor audio remains **FINAL AUDIO PENDING**.

### Access and verification

Public HTTP and official Poly Haven APIs worked. Tabbit's installed CLI returned
exit code 1 without a usable browser backend; no successful browser session was
claimed. The author's anonymous “free Standard” download flow was accessible over
HTTPS without login, payment or access bypass. No CAPTCHA or quarantine bypass
was performed. A page encoding issue was local and corrected to UTF-8; it was not
a global Internet failure.

Only PNG, FBX, GLB, ZIP containers and plain-text notices were consumed. Checks
include signatures, size caps, publisher MD5 where supplied, local SHA-256 and
allowlisted archive extraction. Blender runs with `--disable-autoexec`; no
downloaded script or installer has been executed. ZIP source is retained, but
only GLB and notices were extracted. Raw external files and derivatives are
separate to keep provenance and rollback reviewable.
