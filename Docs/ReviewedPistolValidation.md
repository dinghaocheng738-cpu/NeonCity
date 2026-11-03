# Reviewed pistol base and character hand contact

Scope: adapt a verified CC0 base for the existing pistol, correct model scale and
contact, and retain the generic weapon/damage/ammunition systems. No new weapon
type or balance change is introduced. Whole weapon art/experience remains open.

## Source and adaptation

OpenGameArt's Pistol by loafbrr_1 was reviewed on its actual asset page and in the
downloaded README before adoption. Both declare CC0 1.0. Full receipt, license
evidence and safe FBX/PNG inventory remain in ArtSource/External/OpenGameArt.
The source .blend and source scripts are not used. See ArtAssetManifest.md.

The separate editable Blender derivative shortens/widens the upper silhouette,
replaces the exposed hammer appearance with an original shroud, changes the grip
inserts/heel and uses the shared lagoon/graphite/coral material direction. It is
21.59 cm long, 12,785 triangles at LOD0, five materials and three UE LODs. Import
checks actual scale and muzzle geometry. Original project artwork remains intact.

## Actual contact evidence

ReviewedPistolPresentation01 and 02 pass normal equip/aim/fire/reload, nonlethal
hit continuity, Downed/restore and equipment reattachment checks. Real PIE images
expose an unresolved wrist/palm offset despite the improved overall model size.
Pose 02 measures the right middle-finger base 7.85 cm forward of the wrist; the
old model origin placed the grip at the wrist. The corrected derivative anchors
the grip in the palm and authors a support-hand socket.

The owned character wrapper adds standard non-stretch left-arm IK in the current
right-hand bone's frame. Only models with a SupportHand socket opt in. Reload,
hit and incapacity blend it out. Vehicle mounts and weapon gameplay remain
independent; no actor transform is forced. New assertions check actual palm and
support contact and the live shot origin, rather than only component existence.

ReviewedPistolPresentation03 passes the stricter live contact checks. Actual PIE
frames 21, 32 and 43 were inspected for aim, reload and returned support contact;
frames are in Saved/VisualOverhaul/ReviewedPistolPresentation03/Motion. The left
wrist is measured at weapon-local (1.5, -1.5, -2.5) cm, matching the authored
support socket. This improves palm/wrist placement, not individual finger posing.
ReviewedPistolRegression02 reports 13 unique successful cases with zero test
errors: all seven weapon cases, weapon assets, combat foundation, persistence
round trip, character presentation, living city and swimming. Its runner rejected
an abbreviated Foundation filter as an unexpected name despite that case passing;
the runner now rejects unregistered exact filters before launching UE.
ReviewedPistolFullRegression01 then passes all 116 unique registered cases in one
queue, with zero test errors, no missing cases and no duplicate names. Its report
is Saved/VisualOverhaul/ReviewedPistolFullRegression01/Regression/index.json.
The queue runs the compiled pistol checkpoint; subsequent marine source work was
not part of that binary and receives its own build/test evidence.

## Retained diagnostics

- PistolEditorBuild01/02 exposed test-only const/TObjectPtr assumptions; corrected
  to the installed UE 5.8 API. PistolEditorBuild05 similarly exposed an editor
  graph-node TObjectPtr iteration; corrected without modifying Engine code.
- ReviewedPistolRegression01: 10 of 11 cases passed. The one failed check compared
  world transforms on an unregistered class default object. Replaced with explicit
  local default coordinates plus a separate live PIE muzzle equality assertion.
- PistolEditorBuild06 and PistolGameBuild02: Development Win64 compilation passed
  with zero compile/link errors. Existing Engine/toolchain warnings remain.
- Authoring-20260926-002345 encountered UE's rooted-expression assertion when
  deleting an unchanged material graph during reimport. The importer now reuses
  verified, unchanged material graphs; geometry updates do not delete them.
  Original packages/receipts are retained and no Engine file was modified.
- Authoring-20260926-002547 safely rejected a 26.86 cm broken derivative. Imported
  source parts were children of the frame, so translating all parts moved children
  twice. The working copy now flattens parents while preserving world transforms
  before editing. Offline FBX and official UE reimport both verify 21.59 cm overall
  length; the assembled source preview and live model were inspected afterward.

Further visual work: precise individual finger contact, long-gun-specific poses,
full four-weapon material/animation consistency, dedicated recovery and final
audio. Offline Blender previews are source review, never presented as UE evidence.
