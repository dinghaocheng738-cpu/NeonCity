# Professional asset pipeline and licence gate

## Discovery before authoring

For each category, search the original publisher's catalogue before rebuilding
geometry or materials. Prefer Poly Haven, ambientCG, Kenney and Quaternius; judge
style/quality independently of price and licence. Search generic vehicles,
aircraft/marine bases, urban/beach props, road/building modules, PBR, HDRI,
animation and lawful audio. Do not replace unsuitable primitives with another
unsuitable low-detail pack merely to mark the category complete.

Record one final category decision: FOUND ONLINE AND USED, FOUND ONLINE AND
MODIFIED, CREATED FROM SCRATCH, BLOCKED BY LICENSE or BLOCKED BY QUALITY, with
reason. Candidate/discovery status is not adoption. Network failure is RESOURCE
SEARCH BLOCKED, never an invented URL/licence; continue independent local work.

## Before download

Verify the actual asset page and applicable licence, including paid/free tiers.
Prefer CC0 1.0 or verified public domain. Explicit commercial-use terms need
modification, game distribution and source-repository compatibility checks.
CC-BY needs its attribution/version recorded and delivered. Reject NC,
editorial/research/personal-only, unknown/no licence, ripped/extracted game
content and protected branding. A site's 'free' label is insufficient.

CC0 copyright permission does not grant third-party trademarks or imply
endorsement. Avoid recognizable branded vehicle replicas, airline/police liveries,
gun marks and protected landmarks. Hero vehicles use reviewed bases/references
and are adapted to NeonCity's coherent original design.

## Intake

Prefer FBX/OBJ/GLB/Blend and PNG/EXR maps; audio WAV/OGG where verified. Archives
are containers only: inspect members, reject path traversal/symlinks, executable
and script payloads, and unpack only intended asset/licence files. Verify file
signatures, record SHA-256, retain original sources separately from edited files.
Never execute downloaded scripts. Blender opens external files with Python auto
execution disabled. Do not install unknown addons/installers.

For every adopted asset ArtAssetManifest records name, author, website, direct
source URL, licence/version/date, attribution requirement, download date, local
source, UE destination, modified/unmodified and NeonCity usage. Preserve the
provider's licence file and a dated verification record. No paid purchase or
account login without user authorization.

## Preparation and import

Fusion is the requested CAD tool for hero hull/design work; verify the local
licence permits this project's use before producing dependencies. Record any
external login/tool limitation rather than disguising a Blender-only deliverable
as Fusion work. Blender handles topology, UV, rig/retarget, poses, wheels/rotors,
interiors, material-slot cleanup and FBX export. Houdini Apprentice is limited to
noncommercial experiments that do not enter production. Substance/ZBrush are
not dependencies.

Use original gameplay actors, collision, stable IDs, snapshots and hardpoints.
Normalize units, pivots, palette, roughness, texel density, detail and LOD budgets.
UE assets are written only through official import/editor APIs. Keep old assets
and reference rollback records. Validate normal game use, not just an asset
viewer or a passing import. All hero assets require silhouette/proportion,
surface, animation, collision, in-game appearance and performance review.

## Primary licence sources checked 2026-09-25

- https://polyhaven.com/license — CC0 assets; website content is separate.
- https://github.com/Poly-Haven/Public-API/blob/master/ToS.md — live API permits
  commercial use, requires identifying User-Agent and provider acknowledgement.
  Discovery tools label Poly Haven; no runtime network dependency is introduced.
- https://docs.ambientcg.com/license/ — CC0 1.0 assets; attribution optional.
- https://kenney.nl/assets/car-kit — asset-level CC0, style suitability separate.
- https://quaternius.com/packs/universalanimationlibrary.html and the publisher's
  itch.io listing — CC0; Standard is free, Pro/Source tiers are separate purchases.
