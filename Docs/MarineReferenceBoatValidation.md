# Canonical working boat — integration evidence

Phase 20.5c, TIDEWATER PARITY TARGET. **Partial integration; not accepted full parity.**
Reference: `dgreenheck/tidewater@4811ba48d795197de5621985f404e765c0b7c0ef`.
NeonCity name: **Harborline 82**. No existing speedboat asset is replaced.

## Geometry, material and physics provenance

The actual MIT `BoatModel` constructor supplies 46,845 triangles in twelve parts,
seven surface programs, retained moving-part pivots, 23 contact shapes and 32
hydrostatic samples. The source's older approximate triangle comment is not used
as the asset measurement. `ArtSource/VerticalSlice/Tidewater/Boat/Source.json`
records each source module hash; immutable source arrays, editable Blender, FBX,
native import receipts and complete MIT notice are retained.

Source metres (+Z bow, +X port, +Y up) map to UE centimetres `(Z,-X,Y)*100`.
Torque/angular velocity use the corresponding axial-vector reflection, not the
polar-vector mapping. Five full-precision UV channels retain source UV, auxiliary
material data and source-local position. Linear vertex colours and custom normals
survive FBX; three native LODs retain the source indexed triangles. The isolated
helm chair (1,559 vertices, no triangle cut) is translated 40 cm toward the wheel:
the source has a camera-only driver, whereas NeonCity retains a seated body with
finite arm reach. Its collider, hip and eye anchors move consistently; no bone
length, hull dimension or hydrostatic profile changes. Collision comes
from the source contact shapes, not a convex hull enclosing the wheelhouse.

Seven bounded WGSL-to-HLSL surface adaptations pass standalone DXC and native UE
material compilation. Native Lumen supplies the indirect lighting. Source display
emission is converted to a daylight-readable native luminance; night calibration
is still required. Four actual rendered views in
`Saved/VerticalSlice/TidewaterBoatNativeReview02/` were inspected. These are an
unsaved asset-review world, not gameplay acceptance.

`ReferenceBoatHydrodynamics` faithfully evaluates source buoyancy, damping,
resistance, keel, propeller, rudder and added-mass forces. Fourteen differential
cases compare with the actual locked JavaScript controller at 1e-7 absolute
acceleration/thrust tolerance and 1e-9 immersion tolerance. Chaos integrates the
forces and resolves real contacts; no per-tick actor-transform assignment occurs.
The reference model currently represents this fixed 3,204 kg hull. Other hulls
must supply their own geometry/force profile rather than changing its mass alone.

## Native gameplay integration

- Shared marine camera settings provide a real helm eye, V first/third-person
  toggle, mouse look and 55% horizon stabilization. Character template movement
  remains unchanged. The reference 62-degree vertical lens maps to approximately
  94 degrees horizontally at 16:9. Marine mounted-weapon safety moves to B; V
  cannot arm a weapon. Ground/aircraft weapon controls retain their existing keys.
- F boarding performs one collision-checked placement. Deck walking uses native
  CharacterMovement based movement on welded source floor shapes. Roof/rail/side
  contact classification remains separate from valid floor support.
- F leaves the helm onto a checked deck position with inherited point velocity.
  The engine stops delivering thrust; the boat keeps coasting under hydrodynamic
  forces. Welded floor departures use root-body point velocity and honour UE's
  linear/angular inheritance switches.
- Contact shapes do not add solid-box mass on top of the authored displacement.
  The hull retains its finite source mass/inertia and unconstrained pitch/roll.
- A stable-ID deck station currently reuses the real fishing fight/inventory
  path. Once cast, the float stays at its water location instead of being towed
  by the boat. **This fixed station is transitional: safe-deck R equip, charged
  cast, source rod presentation and the full fishing-parity scope remain open.**

## Evidence and diagnostic boundary

`TidewaterBoatRuntime07` first passed the ordinary-input loop in 36.32 seconds:
water boarding, walk to helm, F possession, V helm view, W drive, V chase view,
F leave helm, five seconds of native based movement, cast/bite/strike/fight/catch,
keep one fish, walk back and resume helm. HUD captures and frame/stage records are
under `Saved/VisualOverhaul/TidewaterBoatRuntime07/`. The deck-catch image was
inspected. Runtime06 helm/chase images were also inspected. This automated test
starts with a deliberately positioned player/boat fixture and is **not** the
final continuous South Beach-to-island player recording or a human feel review.

The later `TidewaterBoatWorld01` and `TidewaterBoatSeat01` runs use the actual
South Beach marina Harborline berth `(4820,-4450,-130)`, yaw -90, instead of
spawning/repositioning a boat fixture. A deliberately positioned player starts on
the existing pier, walks its steps into water and approaches the real gunwale.
The complete loop passes in 38.32 seconds in Seat01; all ground/marine/air driver
contact checks pass in 32.55 seconds. World01 retained a two-hand reach failure
on the source chair; the documented chair translation fixes it without relaxing
the 4 cm hand/foot and 8 cm hip checks.

`TidewaterBoatWorldCapture01` passes that same real-berth path with frame/stage
evidence. Its three actual helm, chase and deck-catch images were inspected.
Current ocean appearance, transitional fixed fishing station and template player
remain visibly incomplete parity items. No combined island route is claimed.

`TidewaterBoatRestart01` passes separate A and B OS processes. Both the original
speedboat and Harborline restore stable IDs, data profiles, ownership, distinct
health values and transforms from the same disk save. The isolated test slot is
removed by B. `TidewaterBoatWorld01` also passes ordinary marine persistence and
boat-mounted combat, including V camera / B weapon separation. Editor15 and
Game02 compile/link successfully; the asset ledger verifies 514 managed packages.

Earlier failures are retained: welded shapes initially doubled hull mass; a
coastal fixture grounded on an existing sand mesh; a test initialized falling
after entering water and overrode native swimming; a cast target lay beyond the
existing bounded water volume. Correcting the mass contribution and fixture
assumptions resolved these without disabling physical contacts or swimming.

## Remaining parity work

`TidewaterBoatFullRegression01` ran all 133 registered cases: 130 passed
(including two cases with engine warnings), three failed. The original Harborline
berth obstructed the speedboat's departure and the existing swimming return route.
The affected cases were `Marine.DockDrivingPIE`, `Marine.ThrottleLoopPIE` and
`Visual.SwimmingRoutePIE`. These failures are retained rather than weakening their
assertions. The berth moves east to `(6000,-4700,-130)`, yaw -90. The new ordinary
water approach and all three previously failing cases pass in
`TidewaterBoatBerth02`: departure 16.92 s, reference loop 43.84 s, throttle 35.27 s,
swimming 46.26 s; zero errors, one engine warning. The subsequent
`TidewaterBoatVillageRegression02` passes all 134 registered cases in 1573.90 s,
zero errors, missing, unexpected or duplicate cases. Two engine warnings remain:
render-thread motion-vector console access and a non-Nanite VSM marking-queue
overflow in the coastal fishing view. They are not visual/performance acceptance.

Fuel/upgrades, mooring, any-safe-deck fishing, waveform buoyancy/wake/spray,
source flag/antenna vertex motion, night lighting/audio and combined performance
remain to be completed. Source windshield geometry is an open frame with wipers;
only the compass has a glass surface. This difference must remain explicit until
native windshield treatment and its reflections are reviewed. Island, village,
reef, wildlife and remaining NeonCity 20.5c workstreams remain in scope.
