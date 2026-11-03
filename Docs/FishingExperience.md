# Coastal fishing — implementation and acceptance in progress

This is a bounded part of Sections 47–53, not completion of the marine or whole
vertical slice. Aquarium, sea pens, final close-view art/animation and audible
quality still require work. No new world region, player movement rewrite or
instant F-to-fish reward is introduced.

## Runtime and flow

FishingComponent is a local controller activity adapter, independent of template
CharacterMovement and Combat. The existing interaction trace discovers authored
FishingStation points by interface. Stable station IDs register weakly with the
game-instance FishingSubsystem; duplicate IDs, other worlds, distance/visibility,
movement, health, equipped weapons and invalid water targets are checked before
starting. The existing MarineWorldSubsystem supplies the cast water surface.
Only the active local point is checked; there is no per-frame citywide search.

The player finds the pier using Phone / N / Coastal Life / Places, stows their
weapon, approaches the sign and presses F. Left mouse casts, a visible float
waits for a bite, and a new press hooks it. Holding reels, releasing reduces
tension. Early hooking, missed bites, excessive tension, loss of the fish and
interruptions have separate failure states. A catch requires explicit Keep (1)
or Release (2). F/Escape exits; P leaves before opening the phone. All weapon-slot
inputs are consumed only during this activity. Movement input uses a balanced
controller ignore stack, never a player transform constraint or altered movement
implementation. Damage/incapacitation, leaving the point, possession, water loss
and load release the activity and its visual/audio components.

Pure FishingSessionModel contains no rewards or inventory. It is advanced in
bounded 50 ms steps, preventing a hitch from skipping an entire bite window.
FishData assets define three fictional coastal species, weights, sizes, rarity,
habitat, time/weather preferences and sale value. Current weather sampling uses
Clear until a concrete weather provider is integrated; no live weather effect is
claimed. Catches store GUID, species FName, dimensions, UTC time, station ID and
storage identity as values. Pouch capacity is 64; total inventory is bounded at
128 and cooked food at 16.

Kept fish can be sold through the physical fish market or grilled over elapsed
time at the public grill. Sale removes the item before the authoritative economy
credit notification; a failed credit restores the same item. Inventory mutation
guards reject nested writes and save/load callbacks. Grilling consumes one fish
into one stable meal. Eating on foot through the phone restores at most 25% of
maximum health and 20 stamina, does not revive an incapacitated actor and does
not waste food when already full. Existing legal stamina above 100 is preserved.

Coastal Life implements the optional PhonePageInterface. The shared phone routes
page input/text without owning fish rules. Other phone apps retain their current
handlers. Left/right browses and Backspace changes Pouch/Food/Places; Enter sets
guidance or eats the selected meal. Navigation does not silently claim a road
route across the shore; the destination is a stable authored activity point.

## Presentation and persistence

The owned character animation wrapper adds opt-in non-stretch two-hand contacts.
Epic land locomotion remains linked. Equipment transforms and a three-segment
line are visual only. The float follows the cast and bite; the landed catch
shows its authored mesh/size; cooking displays the selected fish at the grill.
This does not yet certify final wrist/finger, casting or cooking animation.

CC0 sources, unsuccessful preview/import iterations and derivative decisions are
recorded in FishingResourceSearch and ArtAssetManifest. The rod and fish/facility
bases have immutable raw copies, reviewed derivatives, official UE imports and
hash ledgers. New reel and splash audio previews have verified CC0 provenance;
mono/fade/loop preparation passes signal checks, with perceived sound quality
still pending. No source script/installer, branded asset or game extraction is used.

SaveGame's optional tagged Version 1 HasFishing/Fishing section contains only
kept fish and meals. Old V1 files receive empty defaults. Envelope validation
bounds records; normalization warns and skips missing species, invalid IDs,
malformed values and duplicates. Runtime rod/line, pending catch, timers, input
locks and activity actors never serialize. Load stops the activity before player
restoration and restores fish after world time. Existing SaveSubsystem remains
the only disk boundary. No tick or per-cast disk save is added.

## Evidence so far

- FishingEditorBuild01 failed on local C++ array/TObjectPtr deduction and UWidget
  member-shadow names. Corrected without relaxing build settings.
- FishingEditorBuild02 succeeds. FishingRules01 passes InputRules and
  ProfileAndSchema, including 32 input strategies, failures and legacy defaults.
- Blender source review corrected missing external texture, floating eye/gill
  details and clipped facility supports. Raw sources remain untouched.
- Official authoring attempts at 06:00 and 06:04 failed on Socket/DataAssetFactory
  property assignment; corrected to the supported set_editor_property API.
  Authoring-20260926-060634 successfully imports art/data/graph/map.
- FishingEditorBuild03 succeeds; FishingCoastal01 reaches the actual phone page
  but stalls while walking through an existing parked vehicle at approximately
  (5753,1206,137). This is a failed normal-route run, not a successful fishing loop.
  The route now goes around the parking row; extra stage/timeout evidence added.
- Full normal-map catch/sell/cook/eat flow, actual hand/UI/audio review, separate
  process restart, Game build and complete regression remain pending.

- FishingEditorBuild04 succeeds; FishingCoastal02 completes three catches, two
  explicit keeps, one release and disk save/load, then crashes in a new test
  fixture that adds a TArray element back into its own array by reference. The
  fixture now copies the value first; production validation was not relaxed.
- Its actual images expose a sideways rod and a water volume extending beneath
  the beach. FishingEditorBuild05/06 add open-water clearance and facing checks;
  official Authoring-20260926-062734 moves the point down the existing pier.
- FishingCoastal03 passes those checks, catches and reload, but its return route
  stalls against the existing promenade lamp at (5200,-1043,144). The route is
  corrected around the lamp; existing collision stays intact. Actual images also
  motivate an opt-in offset camera and moving the sign beside the fishing stance.
  FishingEditorBuild07 succeeds; the next visual/route validation is pending.

State: **PLAYER EXPERIENCE NOT COMPLETE / ART PASS NOT COMPLETE**.

## Direction-adjustment snapshot — 2026-09-26

- FishingEditorBuild08 and FishingGameBuild01 succeed (Development Win64).
- FishingCoastal04 and FishingCoastal05 each pass the complete ordinary input
  route: home exit, walk, three casts/fights, two keeps, one release, market sale,
  three-second grilling, phone meal consumption and disk restoration. The
  stamina 40 condition used for food consumption is an explicit fixture; no
  natural travel fatigue is claimed. UI/wait/reel/catch/market/grill/phone views
  are actual 1600x900 PIE frames. Fish are visible after allowing presentation
  frames to settle. Audio-enabled tests check playback state, not human listening.
- FishingCrossSession01 passes separate A/B processes 27212 and 11440 with the
  same SaveID 690B636D48888E6A6C89F9A51F4891C8, exact two-catch signatures and
  cash 1850. Repeated load adds no fish or reward and leaves no activity lock.
- FishingFullRegression01 is RUNNING at this snapshot, registry 129. It must not
  be reported as passed until the full report is checked.
- Source audit validates 142 generated files, 64 external files (including
  newly evaluated aquaculture candidates) and 200 managed packages.

The user's Tidewater amendment increases the requirements. This shoreline
checkpoint does not implement charged casts, region/depth ecology, personal
records, unrestricted deck fishing, a drifting-boat loop or the ported island.
It is a preserved working increment, not satisfaction of that amendment. The
existing code will be extended after reading the pinned Tidewater implementation;
no parallel replacement system will be created. See the retained requirement
Phase20_5c_TidewaterUpdate.txt. All earlier failed runs remain as evidence.


## Full fishing regression closeout — 2026-09-26

FishingFullRegression01: 129/129 unique registered tests succeeded, zero errors,
missing, unexpected or duplicate tests. Seven warnings: one Engine motion-vector,
one VSM queue overflow, three fishing spline attachment and two spline-material
usage/recompile warnings. The spline warnings are actionable and are being fixed;
this result is not a zero-warning or visual parity acceptance.

The highest-priority request is retained in Requirements/Phase20_5c_TidewaterParityUpdate.txt.
TIDEWATER PARITY TARGET covers the entire reasonable marine/island feature set.
See TidewaterParityChecklist.md and ThirdParty/TidewaterIntegration.md. Overall
Phase20.5c remains IN PROGRESS; no Phase21 work is authorized.
