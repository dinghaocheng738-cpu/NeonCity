# Interior asset intake review — PF-015 / PF-020 / PF-021

Review date: 2026-09-27. Research is not an imported or accepted interior.
Three individually reviewed models were subsequently downloaded for evaluation;
their immutable FBX/2K PNG sources and receipts are under `ArtSource/External/PolyHaven/`.
No new furniture has been imported into UE or placed in the room. Existing
licensed inventory is retained. This prepares PF-014–PF-018 while the connected
coast/island regression runs.

Primary sources checked:

- [Poly Haven furniture catalogue](https://polyhaven.com/models/furniture)
- [Poly Haven asset licence](https://polyhaven.com/license): CC0 assets,
  commercial use and modification permitted; attribution not required for assets.
- [Poly Haven API terms](https://github.com/Poly-Haven/Public-API/blob/master/ToS.md):
  current terms permit commercial API use; a distinct application user-agent is
  required. Live API presentation has its own credit requirement. Downloaded CC0
  assets and the API service are different rights/usage boundaries.

| Asset / author | Direct source | Licence | Design assessment | Intake state |
|---|---|---|---|---|
| Modern Arm Chair 01 / Vibrant Nordic | https://polyhaven.com/a/modern_arm_chair_01 | CC0, official asset page | Contemporary wood/leather form is a candidate for the apartment living zone; inspect brand resemblance, material scale and native LOD before adoption | Source downloaded and geometry audited; UE quality pending |
| Modern Coffee Table 01 / Amin | https://polyhaven.com/a/modern_coffee_table_01 | CC0, official asset page | Wood/stone palette could fit the same room; must be sized around the player's circulation and sofa spacing | Source downloaded and geometry audited; UE quality pending |
| Modern Wooden Cabinet / Patrik Pangerl | https://polyhaven.com/a/modern_wooden_cabinet | CC0, official asset page | Contemporary timber storage; 2.44 m width must fit a planned wall rather than block circulation | Source downloaded and geometry audited; UE quality pending |
| Sofa 03 / Fran Calvente | https://polyhaven.com/a/sofa_03 | CC0, official asset page | Ornate Victorian carving/fringe is inconsistent with this modern starter apartment | BLOCKED BY QUALITY (style fit); not downloaded |
| Modern Ceiling Lamp 01 / James Ray Cock | https://polyhaven.com/a/modern_ceiling_lamp_01 | CC0, official asset page | Simple glass pendant; check 1 m drop against room head clearance, and translucent/emissive split | Candidate; not downloaded |
| Desk Lamp Arm 01 / Kuutti Siitonen, Yann Kervran | https://polyhaven.com/a/desk_lamp_arm_01 | CC0, official asset page | Articulated work lamp; 26K triangles is excessive for a small distant desk item without LOD | Candidate; not downloaded |
| Electric Stove / Kuutti Siitonen | https://polyhaven.com/a/electric_stove | CC0, official asset page | Compact four-plate cooker could suit a modest kitchen; review weathering and visible markings | Candidate; not downloaded |
| Throw Pillows 01 / Serhii Khromov | https://polyhaven.com/a/throw_pillows_01 | CC0, official asset page | Woven cushions; strong red/orange pattern needs restrained use or material adaptation | Candidate; not downloaded |

The existing `ArtSource/External/Kenney/FurnitureKit` is a legal local inventory,
not automatic approval of its visual quality for a hero interior. Do not fill the
room with miscellaneous kit pieces just to increase object count.

The current [Quaternius licence](https://quaternius.com/license.html), QAL v1.0
dated 2026-08-28, permits commercial products but restricts standalone asset
redistribution. Some older furniture pages still display CC0. No new Quaternius
furniture was acquired under an assumed CC0 licence; resolve the individual
download's licence before intake. Existing evidenced historical acquisitions
are not relabeled by this research.

The official Poly Haven lighting, appliance, kitchen and decoration API catalogues
were queried with the required application user-agent. Metadata snapshots are in
`Saved/VerticalSlice/PF015InteriorResearch01/`. Discovery is not permission to
download every item or automatic style acceptance. Bathroom, modern sofa, bed,
plant, rug and complete kitchen module selection remain open.

CPU-only Blender FBX inspection (`PF015InteriorMeshAudit01/Meshes.json`) confirmed:

| Model | Actual dimensions (m) | Triangles | Import consideration |
|---|---|---:|---|
| Chair | 0.820 × 0.987 × 1.023 | 8,916 | Two material slots: timber and cushions |
| Table | 1.202 × 0.600 × 0.390 | 4,504 | One material slot; floor-level origin |
| Cabinet | 2.440 × 0.520 × 0.680 | 24,976 | Body plus two sliding doors; omit zero-triangle path helpers in a future derivative |

Every dimension agrees with the source API to within 1.5 cm. The audit used no
downloaded script, rendered no promotional image and made no map changes.

Before any asset download, record the per-asset author, direct source, actual
licence/version and commercial/attribution decision in `ArtAssetManifest.md`.
After intake, add date, verified file types, hashes, source/UE paths, modifications
and usage. Evaluate native UE screenshots and collision/performance before claiming
FOUND ONLINE AND USED or FOUND ONLINE AND MODIFIED.

Apartment planning must cover living, kitchen, sleeping, storage and bathroom
access. Reserve a continuous entry-to-interaction route before placing furniture;
daylight/window depth and artificial night lighting are required. Lamps, kitchen,
appliances, bed, rug, shelving, plants and small functional details still require
their own source and quality review. No furnished-interior PASS is claimed here.
