# Direct ocean FFT adaptation — in progress

This work continues the authorized Phase20.5c direct Tidewater port after the
physical forest checkpoint. It is not a new phase, an accepted ocean image, or
a replacement for the still-open forest daylight balance and continuous route.

## Reviewed source and immutable intake

Source: `dgreenheck/tidewater`, commit
`4811ba48d795197de5621985f404e765c0b7c0ef`, `src/ocean/OceanFFT.js`.
MIT copyright/notice are retained in `Docs/ThirdParty/Tidewater-LICENSE.txt`, the
intake directory and every derived program. No new third-party binary asset is
downloaded or run. `ExportTidewaterOcean.mjs` checks the clean locked checkout
and notice hash before constructing the original source OceanFFT. Only GPU
pipeline creation and dispatch are intercepted; constructor equations, default
parameters and generated shader bodies remain the original implementation.

`ArtSource/VerticalSlice/Tidewater/Ocean/Source.json` records all source/derivative
hashes, nine original compute bodies, sampling/common/uniform programs, the
224-byte uniform layout and five original constructor profiles. Defaults are
four 256-square cascades with tile sizes 733, 157, 33.3 and 7.1 metres. Local
wind and long swell remain separate JONSWAP systems with TMA depth correction,
directional spreading and source PCG/Box-Muller initialization. Profiles also
exercise zero spectral energy, light wind, strong wind and 12-metre depth.

## Shader adapter

`PortTidewaterOcean.mjs` converts the captured WGSL syntax to HLSL without
substituting the wave model. Row/column butterfly stages, complex packing,
positive-angle unnormalized inverse FFT, checkerboard sign, Jacobian whitecap
generation, persistent foam and both nine-level mip chains remain intact.
The original source uses float32 workgroup/buffer intermediates even while
writing RGBA16F mip textures; later levels must not average already-quantized
half-float data. Mips 1–5 are generated per 32-square tile; 6–8 use one group
per layer. Initial spectrum setup uses three dispatches, a normal frame six.

Strict standalone DXC `cs_6_0` compilation (`-Ges -WX`) passes all nine programs
in `TidewaterOceanShaders05`, without warnings. This alone is only a syntax/type gate.
It does not certify UE dispatch, numerical equivalence, buoyancy, performance,
water appearance or completion of the Ocean row in TidewaterParityChecklist.

## Numerical verification prepared

`ServeOceanOracle.py` serves only the reviewed shader/parameter files and a
local test page on loopback. It cannot expose the project tree or overwrite
earlier output. `OceanOracleBrowser.js` runs the original WGSL, capturing full
frequency/row/displacement/derivative fields, accumulated foam and both complete
RGBA16F mip chains. `ValidateTidewaterOceanOracle.py` compares those outputs
against the source spectrum equations and an independent NumPy inverse FFT.
The browser route was blocked as recorded below. The independent compute
runner and native UE execution have now both run; results are recorded below.
Neither a compute capture nor the browser harness is presented as an Unreal
beauty render.

## Runtime integration rules

- The ocean renderer owns one shared four-cascade simulation per relevant
  world, with pooled GPU resources and lifecycle-safe teardown. Do not add an
  independent wave approximation per boat or character.
- Source X/Y/Z metres map to UE X/-Z/Y centimetres; choppy displacement and
  normal axes must use the same conversion for rendering and gameplay queries.
- Tidewater WaterQuery uses two inverse-displacement iterations to recover the
  Eulerian surface, plus a 0.35-metre finite-difference normal. Its bounded
  64-slot query buffer has asynchronous CPU results, not a full texture readback
  every frame. Keep query age/issued positions explicit for physics consumers.
- Shore attenuation, bathymetry, wakes, spray, reflection/refraction and the
  visible ocean surface remain separate pending direct-port components.
- Existing flat-volume fixtures must continue working. Native numeric and
  actual boat/swim/underwater tests precede any map integration or visual claim.

Phase20.5c remains **IN PROGRESS**. No Phase21 work has started.


## Development tools

The browser audit could not start: the installed Tabbit launcher returns 69,
and its normal browser executable reports an invalid side-by-side configuration.
No browser installation, endpoint registry or permissions were modified. This is
not a successful browser/source GPU run and is not a network-access failure.

A separate compute-only runner, RunTidewaterOceanSourceGPU.py, can execute the
same unchanged WGSL through official wgpu-py/wgpu-native without a browser.
The isolated runtime is Saved/VerticalSlice/TidewaterOceanRuntime032, outside
shipping assets and engine installation. Official PyPI release wgpu 0.32.0 uses
BSD-2-Clause; bundled wgpu-native uses MIT/Apache-2.0. Wheel URLs, versions and
SHA256 are recorded in TidewaterOceanTooling.json. This is development tooling,
not a new game dependency or a claim that UE dispatch works. No GPU execution
is attempted alongside the frozen 151-case regression.

Sources: https://pypi.org/project/wgpu/0.32.0/ and
https://github.com/pygfx/wgpu-py/blob/v0.32.0/LICENSE .


## Actual source and native GPU verification (2026-09-27)

`TidewaterOceanSourceGPU03` executes all nine original WGSL kernels through
wgpu-native/D3D12 on the RTX 5060 Laptop GPU. It captures eleven frames across
five unchanged constructor profiles plus an explicit foam diagnostic (bias
1.1 for generation/accumulation, then 0 for decay). All 243 full frequency, row,
surface, mip-intermediate and texture fields are retained with SHA256. Both
complete four-layer RGBA16F arrays include mip levels 0–8.

The initial NumPy comparison failed at one tiny spectrum coefficient, repeated
in the default, shallow and foam profiles. `TidewaterOceanInitProbe01` publishes
intermediates from an explicitly separate diagnostic shader copy: native atan2
was -2.704958200454712 versus CPU -2.704965353012085 radians at cascade 2,
row 121, column 113. Substituting only that observed angle in the CPU equations
changes h0.real from 0.000082460945 to 0.000082895970, explaining the original
0.000000435713 discrepancy (GPU value 0.000082896659). The original shader,
all FFT and surface tolerances remain unchanged. The spectrum absolute floor
is now 1e-6 with the existing 5e-4 relative term; CPU libm bits are not required
from GPU transcendental functions. `NumpyComparison02.json`: **243/243 pass**.
The first failed comparison remains beside it. WGSL floating-point constraints:
https://www.w3.org/TR/WGSL/#floating-point-accuracy .

SourceGPU01 was an encoding failure before execution (Windows default GBK); all
source reads now specify UTF-8. SourceGPU02 executed all fields but its redundant
final queue-completion callback hit a wgpu-py ABI mismatch; it is not reported
as a passed run. Each synchronous staging read already waits, so SourceGPU03
removes only that redundant callback. No installed package was patched.

`NeonCityOcean` is a thin Runtime module loaded PostConfigInit, before global
shader registration closes. No engine files or GameInstance logic are changed.
`FNeonCityOceanFFT` owns pooled source spectrum, wave metadata, persistent foam
and both texture arrays on the render thread. RDG owns scratch buffers and
barriers. One shared simulation instance submits three initialization and six
normal-frame compute passes. There is no active world tick, water material
replacement or gameplay readback yet. The caller must execute each graph before
resetting, destroying or advancing the instance.

`TidewaterOceanNativeGPU01` was finite but **failed numerical equivalence**. The
mechanical adapter had inferred `base` as an integer in both FFT indexing and
the unrelated directional-spectrum function. That truncated a fractional energy
weight. The adapter now preserves the float in its function scope, and the PCG
seed is explicitly uint. This demonstrates why a finite image/dispatch alone
is insufficient. Failed evidence is retained; tolerances were not widened for
this conversion defect.

`TidewaterOceanNativeGPU02` executes the corrected nine HLSL kernels through UE
RDG/D3D12, pooling state across frames. Native calm fields are exactly zero;
foam generates, accumulates to 1.5, then decays to 1.374328 and 1.259186.
The independent NumPy check passes **243/243** on native fields. A direct
original/native comparison also passes **243/243**. Maximum pre-half surface
difference is 4.4167e-5 in source units; worst surface RMS difference is 3.0242e-6.
The cross-backend texture check includes that inherited FFT budget (1e-4) plus
one half-float step (1.1e-3 relative). Each backend's own mip chain is checked
more tightly against its own float32 reductions (2e-6 plus 1.1e-3 relative).
This does not permit a broken mip chain to hide behind a cross-backend budget.

`NeonCity.Tidewater.OceanFFTGPU` is registered for future regressions. It captures
all fields, exercises persistent foam/reset between profiles and checks finite,
flat-calm and bounded physical-scale output. Focused/full verification runners
add the independent NumPy gate whenever this test runs; a UE automation pass
alone is no longer a successful ocean verification. The development Python
environment must contain NumPy (`NEONCITY_NUMPY_PYTHON` or `-NumericPython` to
ValidateOceanNative.ps1); the engine Python installation is not modified.
Full-buffer readback, file writing and texture-copy audit passes exist only in
the editor automation fixture, not a running ocean.

Remaining: bounded asynchronous surface-query integration, source coast/wake
rendering, actual boat/swim coupling, ocean visual parity and GPU performance
under the complete scene. This checkpoint does not close the ocean workstream.

Build/test checkpoint: Development Editor Win64 `TidewaterOceanNativeEditor02`
and Development Game Win64 `TidewaterOceanNativeGame01` pass with zero compile
or link errors. The initial Editor01 attempt caught explicit FVector4 array
initialization and was corrected in project code only. Existing toolchain and
engine deprecation warnings remain. `TidewaterOceanNativeIntegration01` passes
7/7 tests (native ocean, rooted foliage contact, water interface/swimming,
reference boat forces/driving and marine persistence), followed by all 243
independent numerical comparisons. The previous 151-case full tree regression
belongs to 097229a; it is not relabeled as a full run of this new 152-test head.
Provenance passes 143 generated art files, 186 reviewed external files, 599
managed UE packages and 374 Tidewater derivatives. No native map or art package
was changed by this ocean computation checkpoint.
