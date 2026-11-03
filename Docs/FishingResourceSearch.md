# Fishing resource discovery and adopted derivatives

Search date: 2026-09-26, before any fishing geometry authoring. This is separate
from the banking implementation. The initial search was followed by the isolated
evaluations and derivative work recorded below; nothing was automatically
accepted because a site advertised it as free.

- [Quaternius Fishing Rod on Poly Pizza](https://poly.pizza/m/skqPvEIpve):
  the individual page identifies Quaternius, FBX/glTF, publication 2022-06-20 and
  Public Domain (CC0). Candidate for an adapted base; the source pack and actual
  geometry/detail still need checking. Do not call it accepted art yet.
- [Kenney Survival Kit](https://kenney.nl/assets/survival-kit): the official
  80-model 3D pack page declares CC0. Candidate equipment inventory; presence of
  a suitable rod/reel was not assumed. The archive contains fish, a signpost,
  workbench and campfire stand, but no suitable rod/reel.
- [Kenney Watercraft Kit](https://kenney.nl/assets/watercraft-kit): official
  45-model pack declares CC0. The download has not been inspected and is not a
  replacement for the existing physical boats or an accepted yacht hero asset.
- [Fishing Rod by Vavrinec Foltan on Blendkit](https://www.blendkit.com/asset-gallery-detail/fabaf5d8-f936-4695-9bb9-a31eb5e09da2/):
  individual page declares CC0 and a rigged 5,163-polygon rod, but labels it Full
  Plan. No paid acquisition, login or inferred download permission. Access and
  suitability remain unverified.
- [Retro Fish by TrevorDane](https://trevordane.itch.io/retro-fish): the author's
  page explicitly grants CC0 commercial use and offers Blender source/texture.
  Its described retro/PSX style is a likely mismatch for close NeonCity views;
  candidate assessment only, with no download or adoption.

Primary-page declarations are evidence of those candidates' stated terms, not
blanket clearance for every file on a platform. A final decision still requires
actual license/version, contents, geometry, branding and style review. Prefer a
reviewed adaptable base over rebuilding all equipment from primitives. If used,
retain immutable raw sources, derive the coherent NeonCity version in Blender,
record the complete provenance manifest and import through the official UE API.

## Actual intake and visual iterations

Official declarations were checked before acquiring the GLB rod and Survival Kit
ZIP. Actual signatures, sizes, hashes and archive paths were verified; no external
script or installer was executed. The rod CDN returns text/plain, but its bytes
are a valid GLB 2 container. The ZIP's License.txt explicitly links CC0 1.0 and
permits commercial use with optional credit. The rod's individual page states
Public Domain (CC0), with no separate version/date printed; that distinction is
retained in its license evidence instead of inventing a publisher version.

Raw sources, selected extractions and receipts are in
ArtSource/External/Quaternius/FishingRod and ArtSource/External/Kenney/SurvivalKit.
Download UTC timestamps are 2026-09-25T21:13:15.894283+00:00 and
2026-09-25T21:10:33.656632+00:00 respectively (2026-09-26 local).
The first fish preview was invalid because its external colormap was absent.
The safe relative PNG dependency was extracted and the render repeated. The two
fish source files share geometry and differ in scale/palette; they are not two
independent high-detail species.

Raw rod and fish are BLOCKED BY QUALITY as final close-view art. The reviewed
geometry is instead used as a derivative base: contemporary reel/grip/guide
details, fish proportions/eyes/gills and a shared coastal palette. Own Blender
reviews found floating fish details and a facility clipping error; those were
corrected with mesh-surface placement and capped plane cuts. All raw sources
remain unchanged. Decisions for adopted derivatives are FOUND ONLINE AND MODIFIED,
with runtime hand contact, player flow and final art acceptance still pending.
The small float and bounded spline-line segment are CREATED FROM SCRATCH as
technical tackle/effects after the resource search. These are not replacement
hero assets made without searching.

The Blendkit Full Plan candidate was not acquired; no paid access or purchase
was inferred. Retro Fish and Watercraft Kit remain unacquired candidates. This
record does not grant blanket approval to any provider's other assets.
