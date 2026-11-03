# Aircraft power and instruments (Phase 20.5c, in progress)

**HELICOPTER EXPERIENCE NOT COMPLETE.** This working change concerns rotor
power/deceleration and local pilot instruments; it is not final hero art, sound,
weapon-effect or night-search-light acceptance. AircraftPowerEditorBuild01 and
AircraftPowerLoop01 pass. The earlier MarineFeedbackFullRegression01 used the
previous compiled aircraft code and does not validate these source edits.

## Control policy

The existing world-up hover and height assist remains. W/S forward/back, Q/E
strafe, A/D yaw and Space/C vertical control retain their existing driver port.
Shift gradually increases TargetPower from the normalized HoverPower (.55) to
1. CurrentPower approaches that target with a first-order response. Releasing
Shift retains the selected power. It scales horizontal thrust, so speed still
comes from Chaos force, air resistance, collisions and the existing speed cap.

Ctrl reduces the target to hover power and commands horizontal deceleration,
temporarily taking precedence over W/S/strafe. It leaves lift/height assist
available, so slowing to hover is a recoverable operation. Releasing Ctrl with
W held resumes forward thrust at the selected power. This is the existing
assisted flight style, not a new realistic collective/rotor simulation. Engine
start supplies the previous safe hover setting; authored spin-up is not included.
Power changes do not directly assign hull velocity, position or altitude.

Fixed wing overrides the read-only telemetry and rejects the rotor power port.
Its original throttle, lift, stall, braking and torque calculations are unchanged.

## Presentation and persistence

AircraftFlightStatusComponent uses the existing vehicle widget at 10 Hz, only
for the local pilot. It displays speed, ground-relative altitude, actual power
or fixed-wing throttle, flight phase, selected weapon, actual ammunition and
the existing lock state. Weapon names/ammo/locks are read from the original
VehicleCombat/WeaponManager/LockOn interfaces. No parallel combat logic exists.
The aircraft HUD replaces that actor's duplicate debug status while active.
It explains refused in-air/obstructed exits and clears with possession or teardown.

Current/target power and HUD are transient; stopping/unpossessing/loading resets
them. Stable aircraft IDs, ownership, condition, fuel and weapon snapshots retain
the existing SaveSubsystem contracts and Version 1 additive schema.

## Planned verification

PowerLoopPIE extends the historic normal-map helipad route: disclosed player
fixture, walk/F enter, climb, denied in-air F with feedback, stable hover,
normal acceleration/cruise, Shift fast flight, Ctrl deceleration while W remains
held, hover, weapon selection/arm/lock HUD, physical return/landing and F exit.
No aircraft transform or velocity is assigned during that flight route. The
ordinary FlightPIE case remains registered separately.

The existing HelicopterMissileInputPIE also checks displayed lock progress,
confirmed/lost lock and actual remaining ammunition after its real input-launched
missile hits the staged Chaos car. Its one-time aircraft/player/target fixture is
retained and disclosed; it is not the final unstaged continuous acceptance route.

PowerLoop01 reports actual cruise 65.58 km/h and fast flight 104.71 km/h. It passes
the entire takeoff/hover/power/deceleration/weapon-selection/return/landing/exit
route. Actual airborne-exit, fast-flight and deceleration HUD images were viewed.
Readability against bright sand was insufficient, so the shared status widget
now has a restrained dark background and explicit text shadow; this presentation
revision passes AircraftPowerEditorBuild02 and AircraftInstrumentsMissile01.
The inspected Locking and AfterMissile images show clear contrast, real 40%
acquisition progress, and ammunition changing from 2/6 to 1/6. The existing
missile route also passes confirmed lock, loss behind an occluder, refused
occluded fire, reacquisition, actual hardpoint launch and impact damage.
The rendered client is 1264x711 while the
separate world readback is 1600x900, as documented for the marine evidence.

AircraftPowerEditorBuild03 and AircraftPowerGameBuild01 pass without compile/link
errors. Fixed-wing profile checks now explicitly reject coupling to rotor power,
and verify its polymorphic throttle telemetry. AircraftPowerFocused01 passes all
25 unique requested aircraft/fixed-wing/combat/marine/vehicle/occupant/save cases.
AircraftPowerCrossSession01 passes A/B in separate processes 30516 and 31564,
with the same save ID 1BBDCBC742E65538F336DA90C32CF1BF, aircraft stable ID,
condition 0.5, unowned state and transform. Loaded transient power is zero.
These focused cases are a subset of the full suite; A/B are additional executions,
not newly registered test names. AircraftPowerFullRegression01 passes all 118
unique registered cases in one queue, with zero project test errors, no omissions
or duplicate names. Short Blender authoring renders overlapped that regression;
its timing CSV is diagnostic, not final performance acceptance. Final normal
game weapons/light/night route and continuous whole-game acceptance remain open.

Search-light audit: the current spot is root-relative at (180, 0, -100) cm,
while the owned helicopter source authors only a bracket near (133, 0, -56) cm.
There is no dedicated search-light socket in the imported contact derivative.
The light's component/input tests therefore do not prove emission from a finished
housing. Housing, attachment and actual night illumination must be corrected and
visually checked in the aviation art/light pass before full experience acceptance.

## Rotor sound discovery (no intake yet)

- WuxiaScrub's [Helicopter SFX](https://opengameart.org/content/helicopter-sfx)
  declares CC0 on its author page. Its described approaching/fading pass is not
  a steady near-pilot engine loop: **BLOCKED BY QUALITY** for this use, based on
  the description; not acquired or heard.
- Mish7913's [Propeller (Cartoon) Loop](https://opengameart.org/content/propeller-cartoon-loop)
  is explicitly recorded by the author under CC0, but its stated cartoon style
  is **BLOCKED BY QUALITY** for the current aircraft. No download/use.
- qubodup's [Helicopter Rotor Loop](https://freesound.org/people/qubodup/sounds/187681/)
  declares CC0 and says it was extracted from a government video. The rendered
  description does not identify that original video. **BLOCKED BY LICENSE**
  under this project's provenance gate until that source can be checked; no
  download/use. This records an unresolved provenance check, not a finding of
  infringement.
- C-V's [Helicopter circling overhead](https://freesound.org/people/C-V/sounds/510114/)
  declares CC0 and describes a firsthand distant recording with birds and
  neighborhood ambience: **BLOCKED BY QUALITY** for the near-pilot loop, based
  on the description; no download or audible assessment.

The actual author pages were checked 2026-09-26; no helicopter audio is adopted
by this change. **FINAL AUDIO PENDING** remains explicit.
