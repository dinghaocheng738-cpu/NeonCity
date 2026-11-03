# Direct displaced-water queries — computation checkpoint

Scope: the locked Tidewater `WaterQuery` / `WaterSurface` **optional-no-terrain**
composition, matching source behavior without attached shore/wake/terrain
modules. This is not island coast integration, visible ocean parity, or a boat
buoyancy acceptance claim. Existing world water volumes remain unchanged.

## Source and adapter

MIT source: `dgreenheck/tidewater@4811ba48d795197de5621985f404e765c0b7c0ef`,
`src/ocean/WaterQuery.js`, `WaterSurface.js`, `engine/gpu/Readback.js`.
The exact notice is retained in `Docs/ThirdParty/Tidewater-LICENSE.txt` and the
new intake. `ExportTidewaterWaterQuery.mjs` executes the original constructor and
generated function bodies with pipeline creation intercepted. The actual source
named allocator reserves slot zero for camera, reuses named ranges and rejects
overflow at 64. No reference checkout, engine file, map or art asset is edited.

The port retains four cascade samples at explicit LODs 0/0/0/2, source per-cascade
attenuation, global amplitude, two inverse-displacement iterations and 0.35-metre
finite differences for upward normals. Original no-terrain depth/floor values
are 500/-500 metres. They are explicitly **not** a fabricated island bathymetry.
WGSL bodies and parameter layout are immutable; HLSL and 64 deterministic camera/
gameplay input positions have separate derivative hashes. Strict standalone DXC
`TidewaterWaterQueryShaders01` passes without warnings.

## Native resource ownership

`FNeonCityOceanQuery` is render-thread owned beside the shared FFT field. One
64-thread dispatch consumes the existing displacement texture array; every
request uploads at most 1024 bytes. Same-frame results remain on GPU for future
camera/underwater composition. CPU feedback uses at most three 1024-byte staging
buffers, with no blocking wait, flush, texture readback or actor pointer.

The original source has a three-slot generic readback ring but its WaterQuery
wrapper normally permits one pending request. The UE bridge safely permits
three, recording independent metadata per in-flight copy. Each result carries
its exact issued positions, simulation time, client revision and increasing
sequence. Only a newer ready sequence is returned. If the ring is full, GPU
results still update and the CPU copy is skipped; a fourth staging buffer is
never allocated. Reset drops old ownership while sequence order remains
monotonic. Callers must execute any referencing RDG graph before reset/destruction.

Coordinate conversion and named actor leases belong at the forthcoming world
adapter: source X/Z metres correspond to UE X/-Y centimetres relative to an
explicit ocean origin. Delayed heights must be evaluated at their issued
positions/time, never assigned blindly to an actor's current transform.

## Actual numerical evidence

`TidewaterOceanQueryGPU01` runs native FFT and asynchronous queries in Unreal.
`TidewaterWaterQuerySourceGPU01` runs the **unchanged original WGSL** against all
nine mip levels of the previously captured original GPU displacement arrays.
The same 64 float32 positions are independently checked against the fixed input
fixture. Eleven batches cover changing time, calm/light/strong/shallow spectra,
foam history, 0.65/1/1.35 amplitude and shifted sea levels. Total: **704 samples**.

Both original and native queries pass an independent NumPy bilinear sampler,
two-iteration Eulerian solve and normal calculation. GPU texture filtering and
float-coordinate arithmetic are not required to reproduce CPU libm/interpolation
bits: analytical tolerances are 3 mm in height and 0.008 per normal component.
Observed native maxima are 2.323 mm height and 0.006717 normal component. Exact
calm height, zero tilt and the source -500-metre floor have zero tolerance.

A tighter real-source/native GPU comparison permits 1 mm in height and 0.005
per normal component. Observed maxima are **0.6592 mm height** and **0.003722**
normal component; height RMS difference is 0.04856 mm. These are bounded numerical
tests of this input set, not universal accuracy or a visual acceptance claim.
The native FFT's separate 243-field NumPy validation continues to pass.

## Lifetime and failure checks

`TidewaterOceanQueryLifecycle01` executes actual RDG submissions with:

- Three requests in flight and a rejected fourth CPU copy, while GPU results continue.
- Ring drain, newest revision selection and successful reuse on the next frame.
- Invalid zero/65 query counts and a NaN position rejected before dispatch.
- Exact issued position/time/revision preservation and all 704 finite height/normal/floor values.
- Reset between six spectrum profiles and clean release after staging fences signal.

Native automation, 243 FFT comparisons, 12 analytical query checks and 23
analytical/source-comparison checks all pass. Focused/full runners now include
the query numerical gate. Development Editor `TidewaterOceanQueryEditor03` and
Development Game `TidewaterOceanQueryGame01` compile and link successfully.
Editor01 caught a missing project header include; it was fixed without engine
changes. Full TidewaterOceanQueryFullRegression01 now passes all 152 registered
cases in 1752.84 seconds: zero errors, one existing engine motion-vector warning.
The runner also passes 243 independent FFT checks and all 704 query points.
Frozen staged implementation and Editor Ocean / Editor game-module / Game binary
hashes match the retained TestedSnapshot.json. Unlinked surface/foam preparation
is outside this frozen full-regression evidence.

## Remaining integration

World lifetime/origin, named actor sampling leases, latency-aware hull/swimmer
adapters, rendered surface and same-frame waterline inputs remain open. Real
terrain, source shore waves and interactive wakes must be composed consistently
before claiming coastal queries or replacing the current game water.
Phase20.5c remains in progress; the ocean workstream is not complete.
