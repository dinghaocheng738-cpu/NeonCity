# Navigation and Marcus delivery — implementation audit

Status: **NAVIGATION CHECKPOINT VERIFIED / OVERALL PROGRAM IN PROGRESS**. Source
work started while AircraftLightingFullRegression01 ran the preceding binaries.
That queue passed its unchanged 118-case registry; navigation was excluded from
those binaries. This checkpoint adds four normal registered tests (122 total)
and conditional cross-session A/B cases, which are reported separately.

NavigationFocused01 passes 11 unique cases: road query, phone/vehicle input,
destination failure/recovery, both economy checks, four persistence checks and
both property checks. Development Editor Win64 EditorBuild06 and Development
Game Win64 GameBuild01 succeed with no compile/link errors.
NavigationFullRegression01 passes all 122 unique registered cases, with no missing,
unexpected or duplicated entries and no project test errors. Opportunity-choice
source started during this queue is separate and was not part of those binaries.

The current authored Marcus actor offers the assigned Starter Sedan, pays $350
through CreditOnce with stable receipt SouthBeach.Marcus.Transport.001, and
requires the correct possessed car to stop inside its 9 by 4.4 metre zone for a
second. Accepted/completed state already has a snapshot and reload path. Those
reward, speed, vehicle identity and persistence guards must be retained.

The messages currently tell the player to find an east parking area and a yellow
zone on the west road. There is no minimap/direction/distance destination. The
phone displays only the latest message. The historic PhoneDelivery test stages
the car on the final approach; it is useful regression coverage but does not
validate finding and driving from the ordinary parking location.

The authored TrafficRoute exposes its road spline. Its loop follows two coastal
lanes and the end turns; cache these road samples once when the world is ready.
Use that authored geometry for road display and a bounded route query, with a
clearly distinguished direct last segment where a destination is off-road.
Navigation must not run traffic movement, road collision traces or world scans
per minimap paint. A local presentation component can display the current pawn
across possession, so entering a car does not discard the route.

Keep the destination interface generic (stable ID, label, location and optional
weak moving target). The mission owns which destination to publish: first its
assigned vehicle, then the delivery zone when driving it. On exit, show the
assigned vehicle again; on completion clear the active destination and expose
the existing actual economy result. Rebuild transient navigation from the
accepted opportunity after load rather than serializing actors or UI widgets.

Phone and VehicleCombat both map V, with non-consuming actions, at priorities
20 and 2 respectively. Reproduce the open-phone/armed-vehicle combination before
changing it. Phone app browsing must not also arm a vehicle; future menu focus
should be shared input infrastructure rather than putting phone logic into each
weapon class. Closed-phone vehicle controls and driving while reading must keep
their established behavior.

Acceptance requires a new actual input route from the ordinary map: read offer,
accept, follow car marker, enter, follow road route, stop, observe $350 once,
exit, save/restart and verify completed state. Keep the historic staged financial
guard test alongside this new route. Include inaccessible/destroyed assigned car,
wrong vehicle, phone possession changes and load/rebuild recovery. Actual UI
images must be inspected for legibility and overlapping HUD before any visual
or player-experience pass is recorded.

The current source adds a local controller navigation component and a bounded
vector minimap, avoiding a second scene-capture render. Authored traffic splines
are sampled once (at most eight roads and 1,024 points). Road guidance follows
their authored lane direction and uses explicit direct guidance when no nearby
connected road is available. It does not drive traffic or steer the player.
Destination/position display updates at 10 Hz, road queries at most twice a second
after movement, and presentation is hidden behind the open phone. Goal markers
clamp to the map edge, retaining direction and a metre-distance readout.

The Marcus actor now publishes a stable pickup/delivery destination as possession
changes. The existing reward receipt, speed/zone check and save schema are retained.
Unavailable or destroyed assigned vehicles pause delivery without payment and
clear stale guidance; health restoration can resume the same accepted delivery.
Completion uses the actual credited amount and balance for its transient notice.
The existing phone snapshot rebuilds this transient destination on load and clears
it for offered/completed state. No actor pointer or widget is written to disk.

A focused V-key phone/vehicle test reproduced the conflict in NavigationFocus02:
V opened AutoMarket and armed the helicopter. The fix splits always-available P
from an open-phone-only priority-200 consuming context for app keys and combat
keys. It uses Enhanced Input ownership, with no phone dependency in weapon code.
NavigationFocus03 passes the original regression. The expanded case additionally
checks armed fire/lock suppression and restoration of firing after closing.

## Actual route and cross-session evidence

MarcusNavigation01 passes the ordinary-map route from the existing home spawn:
P reads the offer, Enter accepts, save/load rebuilds the pickup marker, the player
walks to the door, F exits the actual apartment, walks to the original parked
sedan, F enters, follows the authored road, stops inside the real delivery zone,
receives $350 once, exits, saves and reloads. The automated driver supplies the
existing throttle/steering/brake ports; it is not a human keyboard driving test.
No vehicle/character transform staging, velocity writes, time dilation, disabled
traffic or collision changes are used in this route. The existing doorway service
performs its normal safe transition. It covers over 70 metres of actual Chaos
driving. Four actual 1600 x 900 HUD/world views were inspected. The reward backing
was enlarged after the first review; the later A capture confirms readable lines.

MarcusNavigationCrossSession01 repeats the full route in process A (2336), then
closes it and starts process B (4844). B loads SaveID
1ED540D440036AF59428D88073A75AFE and assigned vehicle
0889167F430338D9839BAEB8F5500BB3. Matching evidence records condition 1, owned true,
the delivered transform, balance 2200, opportunity state 2 and three messages.
B cannot claim the receipt again, has no stale destination, and displays the
actual completed message. The completed/restart images were inspected. Test slots
are explicitly prefixed NeonCity_Test_; ordinary route cases clean up their slot,
and cross-session B deletes its slot after verification. User saves are untouched.

The separate recovery case injects a destroyed assigned car, missing actor and
wrong possession, verifies no payment/stale guidance, then restores the healthy
accepted snapshot. It also checks generic weak target expiry, stable publisher
ownership and bounded failure notifications. These are disclosed state fixtures,
not substitutes for the ordinary driving route. Loading clears transient notices
as well as rebuilding the route, so a restored car does not retain a paused label.

## Retained iterations and limits

EditorBuild01 failed on a TObjectPtr auto-pointer deduction; EditorBuild02 fixed
that integration error. NavigationFocus01 passed the road query but crashed in
map paint because TArray rejected appending a reference to its own first element.
A copied endpoint fixes the assertion. EditorBuild03 passes. Focus02 then exposed
the actual input conflict; EditorBuild04 and Focus03 verify its fix. EditorBuild05
adds restart/ammunition checks and adjusted reward layout. EditorBuild06 adds the
recovery case and clears stale load feedback. All failed logs remain available.

The guidance is bounded to authored roads: eight roads, 256 samples per road,
1,024 total points; queries are at most twice per second after 1.5 m movement.
It follows directed points on a single connected authored road and uses a labelled
direct guide for off-road/unconnected goals. This is not a city-wide navigation
graph, obstacle avoidance or traffic controller. The current map is north-up and
shows the player, road lanes and active destination. No external map art was
required or downloaded. General opportunity decline/ignore, broader jobs, final
world art, final HUD/audio and the human/long-session acceptance remain open.

NavigationFullRegression01 also records the normal Marcus route without image
readback or a concurrent UE/Blender process. Its 388 sampled frames give wall-time
p50/p95 16.647/17.651 ms (max 18.662) and GPU p50/p95 9.791/11.647 ms (max 12.362)
at the explicit 1600 x 900, Epic, 100% settings. These are 12 Hz sampled timings,
not every frame or a baseline comparison; they do not close the 30–60 minute
whole-game performance gate. Raw CSV and marcus-metrics-summary.json remain in
Saved/VisualOverhaul/NavigationFullRegression01.
