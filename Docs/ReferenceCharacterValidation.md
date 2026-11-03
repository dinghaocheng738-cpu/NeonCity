# Tidewater people in gameplay — 2026-09-26

The user's correction replaces the mannequin appearance with the actual reviewed
Tidewater people. It does not authorize a new handmade human substitute.

## Source and adaptation

Wood_Male_01 (Tidewater joe.glb) is the player; stable pedestrian IDs select that
model or Female_Adult_04 (marta.glb). Both are Microsoft Rocketbox MIT assets,
individually verified in ThirdParty/TidewaterAssetAudit/Rocketbox/Audit.json.
Raw GLBs, original native skins and their fourteen source clips remain intact.
The Microsoft and Tidewater notices already accompany staged builds.

BindTidewaterCharacters creates two separate gameplay derivatives. Original
topology, UVs, face, hat, clothes and textures are retained. Anatomical bind-pose
mapping adapts the skins to the project's existing Quinn skeleton; facial weights
are merged to the head. The head offset retains the source person's neck length.
Body weights are transferred from the nearby existing template surface and
smoothed locally at the shoulders, avoiding rigid panel-like shirt deformation.
Fingers retain their explicit semantic mapping. No template geometry is added to
the visible person. Maximum weights are bounded to eight around the shoulders.

The original character clips contain gestures and idle poses, without the full
game's locomotion/combat/vehicle repertoire. Using the existing skeleton lets the
new skins use existing land/swim/combat animations, grip sockets, physics asset,
ragdoll/recovery and seated hand/foot IK. No hidden animation driver, duplicate
skeletal component, per-frame pose copy or new animation tick is introduced.

The target reference is the unchanged template exported by Tools/Art/AuditProject.py
to Saved/VisualOverhaul/Audit/PlayerReference.fbx. Its SHA is pinned in
CharacterBindings/generated.json; the editable Blender derivatives are retained.
ImportTidewaterCharacterBindings assigns existing materials, skeleton, physics
and post-process settings, and builds three native LODs. It guards generated and
native hashes and verifies all 128 template packages remain unchanged on disk.
The FBX SDK reports a bind-pose reconstruction warning and successfully rebuilds
the bind pose; this warning is retained, not represented as a zero-warning import.

## Runtime integration

CharacterAppearanceComponent replaces only known template placeholders or its
own managed skins. Custom characters stay untouched. Player uses the male skin;
pedestrians refresh once their stable NPC ID is assigned, including spawn points
that initialize identity after BeginPlay. Existing component, animation class,
capsule, movement and combat logic remain authoritative. Palette tinting applies
only to materials which expose that contract; original source textures are used.

TransportOccupantComponent already copies the represented driver's skeletal
asset, so the same person appears in cars, boats and aircraft. A regression
assertion now explicitly checks that identity during normal F entry/exit.
No persistent player data or save version changes are needed for this cosmetic
adapter. The two source people are reused; a new character customization system,
new outfits or a larger civilian library is not claimed.

## Verification status

CharacterBuild02: Development Editor Win64 succeeds in 4.79 s; Development Game
Win64 succeeds in 29.46 s. Zero compile/link errors. Existing engine API and
non-preferred toolchain warnings remain. Float-literal warnings in the touched
palette adapter were corrected.

ReferencePerson01 passes normal gameplay front/back views, movement, jump/landing,
health preservation, both pedestrian skins, and retained animation/physics.
CharacterCombat01/02 pass input equip/aim/fire/reload, hand contact, ammunition,
hit/downed/recovery and persistence reattachment. The first visual inspection
found a stretched neck and pinched shoulders, leading to the binding corrections
above. Those earlier frames are retained as unsuccessful visual iterations.
Occupants01 passes all eight vehicle types plus re-entry, with the same real
character skin and unchanged pelvis/hand/foot contacts; its initial car, fishing
boat and helicopter views were inspected before the final skin revision.

Final ReferencePerson02, CharacterCombat03 and Occupants02 all pass. Normal-game
front/back/run/jump/landing, aim/reload/recovery and car/fishing-boat/helicopter/
fixed-wing seated views were inspected. Shoulder transitions and neck proportions
are improved relative to the retained initial captures. The original low-polygon
clothing detail remains; this is not a new high-resolution human art library.
TidewaterCharacterRegression01 completes all 141 registered tests successfully in
1,644.11 s, with zero test or reflected-class errors. The existing engine motion-
vector warning (LivingCityPIE) and VSM overflow (CoastalLoopPIE) remain; this is not
a zero-warning claim. Numeric passes alone do not establish visual quality. Facial expressions on the gameplay
derivatives are not implemented; the original facial rigs remain in the source
library. This change does not close the mountain-jungle or overall Phase20.5c gates.
