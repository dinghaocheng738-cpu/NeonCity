# Transport occupant checkpoint

Scope: visible seated driver, anatomical reach, interior clearance and lifecycle.
This is not completion of vehicle theft, a whole transport loop or hero artwork.

## Implementation and source

The real character's visible proxy follows the existing vehicle hierarchy and
copies mesh/materials. It adds no collision, mass or independent pawn. Possession,
input, original driver restoration and safe exits remain in the existing road,
marine and aircraft interaction components. One mesh is reused on re-entry.

The source pose is Quaternius CC0 Driving_Loop, already retargeted to Epic Quinn.
An editor-authored graph uses standard two-bone IK without limb stretching.
Vehicle-relative hand and foot targets are independent of movement simulation.

`ArtSource/VerticalSlice/Transport/NeonCityCockpitErgonomics.blend` contains editable
derivatives of the project's owned original artwork. Eight FBX exports preserve
the original exterior scale/physics origin and correct steering placement, seat
height and the fixed-wing footwell. No new external model is used for these small
ergonomic changes; prior category searches are in ResourceDiscovery.md.

The first visual review revealed that source -Y asymmetric controls imported on
UE +Y. Actual UE vertex inspection confirmed it in CockpitHandedness.json under
Saved/VerticalSlice. Revised source controls use the corrected handedness.
Import now checks actual control vertices against expected contact positions.

## Evidence and current status

- Editor: `Saved/VerticalSlice/OccupantEditorBuild08.log`, successful, including
  the corrected repeat-entry fixture.
- Game: `Saved/VerticalSlice/Occupant-NeonCity-BuildFinal.log`, successful.
- Both Development Win64 targets: zero compile/link errors; existing toolchain
  and Engine deprecation warnings remain.
- `TransportOccupants01`: all seven vehicle classes pass normal F entry/exit,
  possession, proxy visibility, pelvis alignment and cleanup. Actual 1600x900
  Epic/100% PIE captures exposed unreachable controls and plane head clipping;
  technical success did not constitute visual acceptance.
- `TransportOccupants02`: strict hand contact checks correctly fail unreachable
  targets with stretching disabled. Feet and canopy bone-height checks pass.
  Controls were then repositioned within natural reach and mirrored correctly.
- `TransportOccupants03`: interrupted by the new repeat-entry fixture's TArray
  self-element insertion assertion. Fixed by copying the weak actor reference
  before growing the array; Editor rebuild succeeded. No game-system workaround.
- `TransportOccupants04`: passed; all seven transport classes plus repeat entry.
  Hand/foot endpoints remain within 4 cm of authored controls/footwells without
  stretching, the plane head clears its canopy, and the cached mesh is reused.
  Actual captures for Sedan, Sports, SUV, Pickup, speedboat, helicopter and plane
  were individually inspected. Hands reach the corrected controls and feet/head
  fit the interiors. Fine finger overlap and exterior quality remain open.
- `VerticalSliceOccupantRegression01`: all **116 unique registered cases passed**
  in one full queue, zero failed cases, zero case errors. The runner verifies the
  required-name list and rejects missing or duplicate cases. This includes the
  corrected living-city test, new occupant test and historic driving/flight,
  marine/deck, damage/destruction, weapon/police and persistence regressions.

The report paths are `Saved/VerticalSlice/<run>/Report/index.json`; actual frames
are in `Saved/VisualOverhaul/<run>/Motion/TransportOccupants`. All unsuccessful
iterations are retained. The diagnostic side camera inspects actual PIE poses;
it is not a substitute for the required continuous normal-game session.

## Remaining acceptance

Steering wheel/lever motion, finely fitted fingers, entry/door animation, NPC
drivers, passengers and theft are unfinished. Current exteriors, glazing,
interior trim and template mannequin still require the later coherent art pass.
Full historic regression and the focused pose/input checks pass for this
checkpoint. A dedicated continuous normal driving/flight occupant review remains
part of the unfinished vehicle experience, along with long-session profiling.
