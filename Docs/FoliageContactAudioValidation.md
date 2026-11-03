# Physical foliage contact audio

A real CC0 bush-contact recording has been reviewed, downloaded from its public
preview and prepared through Blender's official audio decoder. No source script
was downloaded or executed. Asset provenance and alternatives are documented in
ArtAssetManifest.md. Native import, both Development builds, real contact/output tests and the complete
151-case integration now pass. The earlier 150-case forest-ambience run excludes
this addition and had one driving-test failure; that evidence remains retained.

The solver event uses the existing nearby soft-plant contacts and only
moving capsules (at least 35 cm/s and 4 cm deflection target). It emits at most one
strongest plant contact per tracked pawn per 0.25 s, after updating springs. No new
trace or full-forest scan is introduced. A separate event-driven audio component
has no Tick and at most two short spatial voices per vegetation layer. Its 12 m
listener gate and 0.3 s limiter reject distant/no-motion repetitions. Active voices
finish naturally; the pool does not steal or grow. The current two-layer island
adds at most four voices to the eleven source ambience/step/bird voices.

The controlled physical fixture verifies contact events,
stationary silence, bounded allocation and a non-silent native mixer recording.
This is not an authored-island beauty capture and does not certify listening taste.
Trees still do not fracture, fall or uproot; the hard/soft physical limits remain
in FoliageInteractionValidation.md. Dense-canopy light transport is a separate
unresolved visual issue. No Phase20.5c completion or Phase21 transition is claimed.

## Native verification — 2026-09-26

FoliageAudioBuild01 Development Editor Win64 succeeds in 36.70 s, zero compile
or link errors. Native SoundFactory import succeeds (Authoring-220144) and
WoodCollisionData verifies the finite mono SoundWave in the real project.

FoliageContactAudioNative01 passes the actual PIE contact fixture: template
movement is blocked by wood, ordinary tree/palm contact preserves health 100,
soft vegetation peaks at 26.22 cm then settles below 0.5 cm after departure,
roots remain fixed, and curved palm joints cannot be climbed as artificial steps.
Eleven wood bodies are allocated. Real Chaos impact is 1735.21 cm/s and applies
shared vehicle health damage 100 -> 48.73; the car cannot cross the trunk.

Two moving-contact events are emitted, with one peak playing voice; stationary
contact adds none. The real master mixer produces a readable 2.4533 s stereo,
48 kHz PCM16 WAV (RMS -50.0158 dBFS, peak -29.3324 dBFS, 91.14% nonzero samples).
The fixture has no island ambient owner and the car is still distant while this
excerpt records. It does not pass merely on IsPlaying, a WAV header, silence or
one click. This proves native output, not listening taste or full-route audio
acceptance. The exported excerpt duration is not presented as wall-clock route
duration. Evidence: Saved/VisualOverhaul/FoliageContactAudioNative01/Audio/.

The normal game viewport contact and palm frames were inspected. The controlled
fixture has an invisible floor and is not an authored-island beauty view. The
Chaos screenshot does not frame the car; damage/impulse evidence is the native
physics assertions and event log, not that screenshot. No tree fracture, fall,
uprooting, limb-level leaf simulation or haptic-device support is claimed.

## Final pre-regression integration check

FoliageFeedbackFinalEditor / Game Development Win64 builds succeed in
13.95 / 18.16 s, zero compile or link errors. FoliageContactAudioNative02 passes
again after the Nanite attribute correction: bend 26.65 cm, eleven wood bodies,
Chaos impact 1735.65 cm/s, vehicle health 100 -> 48.70. Two contact events and one
peak voice yield real mixer RMS 0.00370228, peak 0.04095459; standing remains silent.
Native shrub-contact and curved-palm frames were inspected. As before, these are
controlled physics fixtures, not scenic authored-island captures. The complete
updated 151-case regression subsequently passes, as recorded below.

## Full integrated result

TidewaterFoliageFeedbackRegression01 passes **151/151** registered cases in
1755.99 s: 150 plain successes, one warning-success, zero failures/errors, missing,
extra or duplicated cases. The sole warning is existing r.MotionVectorSimulation
render-thread access in LivingCityPIE. No VSM capacity warning appears in this
run; this is not a guarantee for all other scenes/settings. No reflected-class
startup errors are present. The frozen staged tree and Editor/Game binary hashes
match TestedSnapshot.json at completion; no implementation changed during the run.

Both original movement and all foliage, vehicle/base, combat, wanted, economy,
phone, property and save tests remain green. The previously failing reverse test
now logs -14.90 km/h and -348.5 cm, with reverse gear, full throttle and four
contacting wheels. Diagnostic logging did not change driving behavior/assertions;
the earlier failure's cause is still unconfirmed, not declared fixed by a rerun.

The independent audio-enabled Native02 recording remains the output evidence:
this full runner disables audio. Replay of 236 original native contact frames is
Saved/VisualOverhaul/FoliageContactAudioNative02/Review.html; its raw timing CSV
and separate mixer WAV are preserved. No interpolated frames, recoloring or
invented synchronized soundtrack is used. Subjective contact feel/listening,
daylight visual balance and overall Phase20.5c acceptance remain open.
