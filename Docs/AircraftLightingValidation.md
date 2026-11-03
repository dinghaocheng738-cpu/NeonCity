# Aircraft search-light presentation — Phase 20.5c, in progress

The ordinary South Beach helicopter previously emitted from a hard-coded root
offset (180, 0, -100) cm while its authored bracket was at (133, 0, -56) cm.
It had no separate optical housing, emitter socket or volumetric scattering.
Its historic passing input/light tests did not prove a complete visual experience.

The reviewed CC0 optical head is adapted into an unbranded 25 cm fitting with
the existing Lagoon/Graphite/Silver palette. Raw source remains immutable. Two
actual local clay views and the adapted render were inspected; the first crop
left a handle fragment and was rejected. The revised crop removes that fragment,
retains the original lens material classification and reduces the fitting to
6,282 LOD0 triangles. ArtAssetManifest records authorship and licence provenance.

The original bracket is partly inside the authored lower fuselage. Merely moving
the old light to that bracket would still bury the emitter in visible geometry.
The official import places SearchlightMount at (184, 0, -53) cm under the
forward nose and LightEmission 10.3 cm in front of the optical pivot. The housing
and emitter rotate together within existing aim bounds. The lens receives a transient dynamic
emission value and turns off with the real spot. There is one mesh and one real
spot per aircraft, no added collision, physics mass or actor tick. Existing
distance culling and local shadow-quality limits remain authoritative.

Official import passed in Authoring-20260926-024029. The editor audit confirmed
the normal map had no height-fog actor. One thin coastal medium was added through
editor APIs: density .003, height falloff .2, exponential maximum opacity .12,
volumetric distance 120 m, scattering distribution .15. Map/package hashes are
updated consistently in the import ledgers. No collision or time defaults change.
Night captures 01/02 exposed excessive amber scattering around the existing
street lamps. Their surface intensity, shadows and time control remain unchanged;
only their volumetric scattering is reduced to .04 in the owned map. A visible
beam, real surface illumination, occlusion and on/off response require actual UE
captures; material/component properties alone do not pass that gate. Weather,
full aircraft art, rotor sound and final normal-game acceptance remain unfinished.

The evidence harness previously rendered the world at 1600 x 900 inside a smaller
Slate client. Runs 01/02 technically passed but their HUD capture sizes (1264 x
711 and 882 x 462) invalidated that visual gate. Runs 03/04 correctly failed the
HUD capture assertion: the custom client was not shown for Slate composition.
No failed evidence was deleted. The opt-in visual/metrics PIE entry now supplies
one borderless 1600 x 900 custom window with no DPI/work-area rescaling, following
the Engine's supported CustomPIEWindow path. RenderOffscreen uses a virtual
platform window; no persistent editor settings or gameplay simulation changes.
Slate must still show/layout that virtual client before capturing UMG. The shared
capture helper rejects any actual result whose dimensions differ from 1600 x 900.

AircraftSearchlight05 passed after EditorBuild05. Its two actual 1600 x 900 HUD
images were inspected: real nose-mounted light, road illumination, bounded aim,
visible air beam and legible pilot instruments. This short route does not yet
prove the beach/ocean/return loop. EditorBuild06 adds that longer input route,
real L on/off ocean comparison, real V/LMB fire while lit, force-driven return,
C landing and F exit. Its initial nearby-player setup and inspection cameras are
disclosed fixtures; return guidance uses the existing aircraft force-input port,
not hull transform/velocity writes. It is not the final unassisted full aviation
route. Pilot instruments also report the actual light ON/OFF state.

AircraftSearchlight06 passed the longer loop, but its 220,000 cd spot clipped
large areas of the sand to white. Run07 used 12,000 cd and a scattering gain of 4;
surface detail returned but the air cone became too faint. Run08 retains 12,000 cd
and uses the supported per-light scattering multiplier 12 (data-valid range
0–16). This deliberately exceeds the Engine editor's suggested slider range;
the runtime setter supports it. It changes only volumetric contribution, not
surface exposure, material colors or post-processing. No artificial beam mesh.

AircraftSearchlight08 passed after EditorBuild08 with five actual 1600 x 900
views inspected individually: road and passing vehicle, aim change, beach props,
ocean ON and ocean OFF. The cone is visible against the night background; road
paint, car silhouette and sand retain visible detail. The shared water material
responds through an actual reflection; full ocean-wave artwork remains open.
The route additionally passes real V/LMB ammunition use with the light enabled,
day reduction, far culling, transient reset, force-driven return, C landing and
F exit with unchanged 600 airframe health. This is a bounded lighting acceptance,
not the full Section 21 machine-gun/rocket/missile/night route or art completion.

Development Editor Win64 build AircraftLightingEditorBuild10 succeeds (the final
test-only change reuses the shared HUD helper in the marine test and initializes
its screenshot size). Development Game Win64 AircraftLightingGameBuild02 succeeds
with the final runtime. There are no compile/link errors. Existing Engine header
deprecation warnings remain; the local uninitialized capture-size warning is fixed.

AircraftHazeEnvironment01 passes and all nine actual road/sand/hydrant views at
10:00, 17:18 and 21:00 were inspected. Near-surface textures and shadows remain
readable; this does not close the known road-marking, pavement or wider art issues.
AircraftWindowMarine01 passes the actual boat throttle/swim/reboard loop with
1600 x 900 HUD captures; cruise and denied-exit views were inspected. The subsequent
Marine02 run also passes using the common size-checking capture helper; all five
actual HUD outputs report 1600 x 900. AircraftLightingFullRegression01 passes all
118 unique registered cases in one queue, with no missing, unexpected or duplicate
names and no project test errors. The runner exits successfully. New navigation
source was written during the queue but is not included in these binaries.

AircraftSearchlightMetrics01 passes without pixel readback, Blender or a second
UE process. At 1600 x 900, Epic, 100%, 514 sampled frames give wall-time p50/p95
16.671/17.708 ms and GPU p50/p95 7.647/9.593 ms. The 287 night samples give GPU
7.258/8.306 ms. A single GPU sample reaches 22.418 ms. These are short-route
12 Hz samples of frame timings, not a complete frame histogram or final 30–60
minute performance acceptance. Ocean ON/OFF holds have only 11/10 samples and
are insufficient for a general benchmark claim. The raw CSV and summary remain
under Saved/VisualOverhaul and Saved/VerticalSlice with the run label.

The completed AircraftPowerFullRegression01 used the previously built power/HUD
binaries and does not include this lighting work. Its timing CSV is diagnostic,
not a clean performance benchmark: short Blender authoring renders ran alongside
that queue. Dedicated final performance acceptance must run without such load.
