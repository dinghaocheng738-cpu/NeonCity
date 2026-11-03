# Tidewater ocean geometry and foam — work in progress

This checkpoint prepares the original open-ocean branch for native rendering.
It does **not** replace the scene water, add coast/wake composition, connect hull
physics, or establish visual parity. No map is changed by these tools.

## Locked source and geometry

Source: `dgreenheck/tidewater@4811ba48d795197de5621985f404e765c0b7c0ef`, MIT,
copyright 2026 DRG Software Solutions LLC. Full notices accompany both intakes.
The existing reviewed source checkout is reused; no new binary art is downloaded.

`ExportTidewaterOceanSurface.mjs` actually executes original CDLOD mesh selection
and camera/frustum code. It captures shader function bodies without executing GPU
work. Settings match App.js: 32 quads per patch, 8 m leaf size, 12 levels,
[-25,25] m vertical bounds, 2.5 range multiplier and 0.66 morph ratio.
The one reusable grid contains 1,089 vertices and 2,048 triangles, with the source
eight-column strip ordering and alternating diagonals. Selection is capped at
1,500 instances per view. Ten original camera fixtures select 16–192 nodes.

The new C++ selector retains double-precision CPU coordinates, source child order,
partial-parent quadrants, positive-corner frustum rejection and stable near-first
sorting. Output node data and morph uniforms are float32. Inputs are explicitly
source X/Y-up/Z metres; planes use `dot(N,P)+W`, not UE FPlane's sign convention.
Invalid input clears the previous mesh. Capacity exhaustion is reported rather
than silently treated as a complete ocean. Native comparison passes all ten
original camera selections (including source order), every grid/index byte and
every packed morph-uniform bit. An independent source-fixture inspection checks
78,851 node pairs without duplicate or overlapping interiors.

## Surface function adaptation

`PortTidewaterOceanSurface.mjs` preserves the source CDLOD morph, displacement
band limiting, cascade attenuation, extra foam sampling and fragment detail.
VS/PS/CS strict standalone DXC checks pass in TidewaterOceanSurfaceShaders01.
These are syntax gates; the diagnostic shader wrappers are not game rendering.
The generated source functions are now registered in an editor-only numerical
audit. They are not yet bound to an in-game ocean draw.

Vertex displacement samples all nine source mips with fractional detail levels;
the future draw adapter must use a trilinear sampler. The query path intentionally
uses integer LODs 0/0/0/2. Both must consume one shared FFT field and controls.

## Original foam intake

`ExportTidewaterOceanFoamMips.mjs` captures the original static mip shader and its
source hash. The prepared bake runs unchanged original foam WGSL and the original
render-based mip shader with the pinned official wgpu runtime. It retains all 11
RGBA16F mip levels, validates finite [0,1] channels and half-float box-filter
rounding, then packages those bytes losslessly as DX10 DDS.

Channels are R density, G fine bubbles, B large mottling and A streaks. Native UE
TextureFactory supports this format and existing mip chains. The prepared import
keeps linear data, HDR half floats, alpha and all original mips; it guards existing
packages against unrecorded changes. The native asset test compares every source
channel bit and verifies runtime pixel format/mip count. Bake/import/test results
are recorded below. Actual shaded ocean integration remains pending.

## Remaining work

Native surface draw and visual evaluation, original water material/shadows/sky,
coastal bathymetry, shore waves, refraction, hull masks, wakes and latency-aware
physical coupling remain open. Full Phase20.5c and human playtest acceptance are
not complete.


## Verified checkpoint (2026-09-27)

- TidewaterOceanFoamSource01: actual unchanged WGSL generation and all 11 original
  mip render passes succeed. Each output is finite, bounded and independently
  checked against box-filter plus one half-float rounding step.
- DDS source: 11,184,956 bytes, containing all original RGBA16F data without
  resampling or compression. UE Authoring-20260927-015908 imports the one new
  managed texture package using TextureFactory; no map changes.
- TidewaterOceanSurfaceNative02: 2/2 native tests pass with zero errors/warnings.
  Mesh/LOD data match the locked source. Every source texture mip channel bit
  matches the original GPU DDS; actual runtime format is PF_FloatRGBA with 11 mips.
- Development Editor TidewaterOceanSurfaceEditor03 and Game
  TidewaterOceanSurfaceGame01 compile and link successfully.
- Provenance: 143 generated art files, 186 reviewed external files, 600 managed
  native packages and 380 Tidewater derivatives pass validation.

Retained failures: Editor01 found a stale engine-comment API name and test integer
width mismatch; project-side fixes compile in Editor02. Native01 found that an
editor texture still returned placeholder format/one mip after merely finishing
its data cache. The corrected audit waits for the actual texture compiler through
TextureCompiler.h before checking it. Editor03 and Native02 pass without changing
the texture, its settings, source mip bytes or assertions. No engine file changed.

The last complete full run is the **152-test query checkpoint 8972f6a**. This
additional code/asset checkpoint has two focused passes; it is not represented as
a new full 154-case regression or as a visible-ocean acceptance result.

## Actual vertex GPU audit (2026-09-27)

`OceanSurfaceGPU` evaluates the translated source vertex functions on the actual
RDG FFT textures with the original fractional-LOD trilinear sampler. The fixture
contains eight grid coordinates for every selected node in ten source cameras,
under default, calm and strong-wind controls: 30 batches, 28,128 vertices. This
is a compute evaluation of vertex functions, not a rasterized water screenshot.

The acceptance chain is now explicit:

1. `OceanFFTGPU` produces fresh native fields and passes the existing independent
   NumPy spectrum/FFT/mip/query gates. Focused surface runs include it automatically.
2. Unchanged original WGSL executes on those exact native half-float texels using
   the reviewed isolated wgpu 0.32.0 runtime. Input hashes and controls are recorded.
3. Native/source outputs allow one micrometre or two float32 ULPs; depth, LOD and
   node size must match exactly. No separate-spectrum centimetre budget is used.
4. CPU CDLOD arithmetic independently validates lag, spacing and morph. Its eight
   float32-epsilon arithmetic allowance propagates through `h*morphK`, with two
   coordinate ULPs, instead of a fixed metre allowance.

Ideal CPU trilinear interpolation remains a diagnostic, not a sampler acceptance
oracle. `SamplingData01`, `MipTagData01`, `GradientData01` and `BasisData01` (all
prefixed TidewaterOceanVertex under Saved/VerticalSlice) isolate actual hardware
filtering. The synthetic basis probe directly returns discrete 1/256 tap weights;
blending separately observed integer mip samples does not reproduce the hardware
fractional-LOD path. Microsoft documents finite texture addressing/filter precision
in [the Direct3D specification](https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#TextureSamplingPrecision).
These measurements are device-specific evidence, not a driver conformance finding
or permission to relax the same-input source/native gate. No driver setting changed.

Retained failed evidence: Native01 found a missing project audit Platform.ush
include. Native02's initial ideal-CPU comparison failed; the original WGSL itself
also differed from that ideal by up to 28.819 mm. Comparing separately generated
source/native FFTs produced a 2.637 mm height difference. Feeding identical native
texels to both backends removes it: displacement height, X/Z, foam and morph data
match bitwise; only the final sea-level addition differs by at most 4.769e-7 m.
The old reports remain failed and have not been overwritten or relabelled.

TidewaterOceanVertexIntegrated01 passes 4/4 native cases, 243 independent FFT/mip
checks, 12 query checks (704 points), and 60 vertex comparisons. The registered
suite now contains 155 tests. Editor TidewaterOceanVertexEditor01 and Game
TidewaterOceanVertexGame01 build/link successfully (22.48 / 16.66 seconds).
This focused checkpoint does not claim a full 155-test run or visual parity.

`ValidateOceanSurfaceNative.ps1` runs automatically after successful focused/full
native queues containing the surface case. Missing runtime or capture is a hard
failure; it never installs dependencies or changes engine files. Set
NEONCITY_NUMPY_PYTHON / NEONCITY_WGPU_RUNTIME for another reviewed local runtime.
