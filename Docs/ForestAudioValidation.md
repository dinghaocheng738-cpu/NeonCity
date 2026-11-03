# Native forest soundscape — 2026-09-26

The already reviewed Tidewater recordings are retained: 43 CC0 source files,
14 native loops and 130 original one-shot cuts. Individual provenance and
attribution records are in ArtAssetManifest.md and the preserved audio audit.
This integration introduces no new downloaded sound.

## Source evidence

ExportTidewaterSoundscape evaluates the actual pinned SoundScape.js at
4811ba48d795197de5621985f404e765c0b7c0ef. Its 162 environment cases cover dawn,
midday, dusk/night, inland/beach/offshore distance and calm/moderate/strong wind.
Six surface-step banks retain original target loudness and channel correction.
The exporter intercepts only WebAudio output; original mixture and bird activity
equations run unchanged. It does not open an audio device or certify listening.

ExportTidewaterCoastQueries evaluates 521 points through the original
coastDistance and seed-7 Noise2D. Native code queries the same analytic field,
not a decorative terrain-height approximation. Both oracles retain source hashes
and reject a changed reference checkout. RuntimeSource.json records their hashes.

## Native adapter

One component on the canonical island vegetation owner uses existing wind,
actual local audio listener and shared day/night clock. Supplemental jungle
layers cannot duplicate it. Four loop beds, three footstep voices and four
spatial bird voices bound allocation to eleven native components. There are no
per-tree audio actors or edits to template input, movement or animation.

Original inland gust, night insect, dawn chorus, bird-bout and loudness rules
are retained. Three hidden songbird perches use the source coast field and up
to eight ground traces when finding a perch; these are not visible wildlife.
A phrase cannot flood an unbounded voice pool. EndPlay stops/destroys all voices.
Grounded footsteps use actual travel with moving-base displacement removed.
Native third-person stride pacing is an explicit adapter, not animation-notify
synchronization. Grass, sand, wet sand and known wooden island/boat floors are
routed; other source surfaces and underwater behavior remain open.

## Verification and corrections

- Initial editor compile failed on an unsupported TNumericLimits API; standard
  numeric_limits fixes the test only. Initial data authoring rejected a class
  without BlueprintType; no partial data asset was saved. Both are resolved.
- The first three-case check passed mixer/coast math and real PIE movement, but
  14 loop-duration checks failed: UE GetDuration intentionally returns the
  indefinite-loop sentinel. The test now checks the underlying Wave.Duration.
  No audio samples or loop settings were changed to satisfy it.
- TidewaterForestAudioData02: 2/2 source/library cases pass. Maximum coast error
  is 0 m across 521 points; maximum gain error is 2.28389297474e-08 across source
  cases/footstep banks. All 144 native clips match duration, channels and looping.
- TidewaterForestAudioNative01 initially reported Success but its exported WAV
  was entirely silent. IsPlaying and file length were inadequate acceptance.
  This run is **rejected as audio-output evidence**, and remains retained.
- The offscreen PIE window never gains focus. Editor focus muting and device
  selection explain the silence. The explicit audio-test path now enables game
  sound/solo PIE in transient play settings and temporarily bypasses app-focus
  muting via au.DisableAppVolume. It restores the cvar and never saves preferences.
- The recording test parses real PCM samples and requires at least eight seconds
  of output, six non-silent one-second windows, RMS > 1e-5 and peak > 1e-4.
  An empty/silent file or isolated click can no longer pass.
- TidewaterForestAudioNative02 passes: normal grounded walking, eight footsteps,
  four spatial bird events, no standing/jumping steps, day/night bed changes,
  eleven allocated voices and player health 100. Real native master-mixer WAV is
  21.248 s, stereo/48 kHz/16 bit; RMS -42.2104 dBFS, peak -26.4132 dBFS,
  99.1237% nonzero samples and 21 non-silent seconds. It is generated game audio,
  not microphone recording. Subjective listening quality still needs acceptance.
- ForestAudioBuild06 Editor succeeds (23.93 s); ForestProbeBuild07 Editor also
  succeeds (6.34 s) after diagnostic cleanup. No compile/link errors.
- File provenance validates 363 locked Tidewater derivative files.

The first Game build and complete 150-case integration result are retained
below, including its failed driving assertion. The preceding committed grass/physics checkpoint passed
all 147 cases; that is not substituted for this integration's complete run.

## Remaining differences

Contact-specific shrub rustles are now supplied by the separate physical-contact
adapter (FoliageContactAudioValidation.md). Wave-phase surf, boat/hull, underwater
filtering, visible-wildlife events and complete fishing audio remain open.
Wind in leaves is not mislabeled as bodies brushing them. The physical foliage
work remains documented separately in FoliageInteractionValidation.md.
Audio parity and remaining Phase20.5c workstreams stay open; no Phase21 transition.

## Integration follow-up (2026-09-26)

ForestAudioGame07 Development Game Win64 succeeds in 33.24 s with no compile or
link errors. The first complete 150-case run, TidewaterForestAudioRegression01,
has a DrivingPIE reverse assertion failure and is **not accepted** as a green
integration checkpoint. At the 1.8 s sample the car had moved backward 116.8 cm
but had stopped (-0.01 km/h). Forward driving, steering and service braking passed.
The cause is under investigation; no driving logic or test thresholds have been
changed to conceal it. That first run used its own frozen snapshot, before the later contact-foley
addition and Nanite attribute correction.

The completed first run contains all 150 unique cases: 146 plain successes,
three warning-successes and one failure, total 1759.09 s. Warnings are the known
motion-vector console access and VSM non-Nanite marking capacity. A fresh
FoliageAudioDrivingDiagnosis01 (unchanged driving behavior/assertions) passes
2/2 focused cases, including WoodCollisionData. Instrumented reverse reaches
-14.90 km/h and -348.6 cm with gear -1, throttle 1 and four grounded wheels.
One earlier curb contact applies 0.76 health damage. This does not establish the
cause of the failed complete run; diagnostics remain for the next full run.

## Final combined checkpoint

FoliageFeedbackFinalEditor/Game Development Win64 builds pass (13.95/18.16 s,
zero compile/link errors). TidewaterFoliageFeedbackRegression01 passes all 151
registered cases in 1755.99 s with no errors and one existing motion-vector
warning. This includes source mixer/coast/library checks, native forest movement,
contact feedback and every historical regression. The full runner is no-sound;
ForestAudioNative02 and FoliageContactAudioNative02 remain the separate measured
native-output evidence. Original silence/failure records are not overwritten.
The driving reverse rerun passes with unchanged logic/assertions; its prior
intermittent failure is not assigned an unsupported root cause.
