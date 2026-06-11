# 06 — Risk Register & Open Decisions

Revised after the adversarial review (`docs/07-plan-review.md`). New/changed items are marked
**(review)**.

## A. Risks & mitigations

### R1 — Billboards (`AutoTransform`) have no USD equivalent — **HIGH**
OSG orients/scales billboards toward the camera transparently each frame
(`ROTATE_TO_SCREEN`, `autoScaleToScreen`). USD has no camera-facing constraint.
**Mitigation:** text labels → viewer HUD overlay (naturally screen-scaled). Icon billboards →
per-tick `xformOp:orient`/scale computed in `refreshDisplay()` from a viewer-provided camera
direction. **Consider** USD `UsdGeomModelAPI` *draw-mode cards* for the simplest always-facing
billboards. **Residual gap:** orientation snaps at sim-tick granularity, not per render frame —
acceptable for a sim tool.

### R2 — Picking fidelity for thin geometry — **MEDIUM**
`TestIntersection`'s ID pass hit-tests thin lines/arrowheads worse than
`osgUtil::LineSegmentIntersector`.
**Mitigation:** `resolveDeep` mode; invisible thick pick-proxy meshes alongside visible
`UsdGeomBasisCurves` (a standard USD-DCC pattern).

### R3 — Qt + Hydra context interop, **per platform** — **HIGH (top technical risk)** **(review)**
`UsdImagingGLEngine` and `QOpenGLWidget` both manage the GPU context. Three platform facts:
- **macOS caps OpenGL at 4.1; HgiGL needs ≥ 4.5.** So the original "fall back to Dummy if
  < 4.5" would **disable 3D on every Mac**. → Use **HgiMetal + hgiInterop** on macOS (the
  `usdview` path); HgiGL on Linux/Windows (Decision D6). Build must include `hgiMetal` on macOS.
- Hydra leaves GL pixel-transfer state that corrupts `QPainter` overlays → QImage→texture blit
  workaround.
- Sharing one `Hgi` across multiple inspectors needs
  `Qt::AA_ShareOpenGLContexts` set before `QApplication` — **Qtenv does not set it (verified
  absent)**; M3 must add it, or give each viewer its own `Hgi`.
**Mitigation:** a throwaway Qt + `UsdImagingGLEngine` spike on **both Linux/GL and macOS/Metal**
before M3; GL-version/backend check at construction; `QT_NO_KEYWORDS` in the plugin build.

### R4 — Per-frame attribute re-author performance — **MEDIUM**
USD writes whole arrays. INET mutates trail/ring vertices and sphere radii each tick.
**Mitigation:** author at `UsdTimeCode::Default()` and **overwrite** (single value — no
time-sample accumulation/memory growth); cap trail length; consider `UsdGeomPointInstancer`
for many moving entities; rely on OpenUSD 26.x targeted mesh invalidation. **Profile at M8/M9**
(30 fps with ~50–100 moving nodes).

### R5 — Inline GLSL shaders — **MEDIUM**
INET's signal wave-fade and floor-fade use `osg::Program/Shader/Uniform`.
**Mitigation:** primary = MaterialX node graph (decomposable to standard `ND_*` nodes; uniforms
→ time-sampled inputs); fallback = CPU per-vertex `displayOpacity`. **Residual gap:** the CPU
fallback loses the in-ring cosine wave oscillation (smooth radial fade only) — acceptable;
MaterialX restores it later.

### R6 — `osg::ImageStream` animated textures — **LOW**
**Mitigation:** static first-frame icon (v1); UV-offset atlas animation later.

### R7 — Geospatial fidelity vs full osgEarth removal — **HIGH (scope)** **(review: reframed)**
osgEarth has no 1:1 path; v1 omits live tiles, terrain draping, declutter, feature symbology,
and building extrusion (`docs/05`). **This is an accepted tradeoff — osgEarth is removed, the
samples are re-implemented, and reduced visual fidelity in some scenarios is acceptable.** Sim
behavior is unaffected. Higher fidelity is the optional M13 (cesium-native), not a prerequisite.

### R8 — INET port scale — **MEDIUM** **(review: corrected numbers)**
`visualizer/osg/` is **~90 files** (not 84); the OSG include surface is **~88 include lines
across ~33 files** (not "~250 sites") — so include churn is modest. Real work is ~7 infra files.
**Mitigation:** parallel-tree strategy; fan ~40 thin ports to Sonnet agents; OSG selectable
during transition. **Decide where INET work lands (Q4).**

### R9 — OpenUSD build/packaging — **MEDIUM**
Monolithic `usd_ms` is larger than the OSG libs; building USD is non-trivial.
**Mitigation:** minimal imaging config (~30 MB, HgiMetal on macOS / HgiGL elsewhere); optional
dependency; prefer prebuilt (vcpkg/conan/Homebrew); document the build.

### R10 — Third-party model breakage — **MEDIUM**
Custom C++ models that built `osg::Node` graphs break at final removal (M12).
**Mitigation:** the `cScene3DNode = osg::Node` alias keeps them compiling during transition;
migration guide; optional migration facade (Q1). This is a deliberate one-time break to kill
OSG — making old OSG model code compile forever is explicitly not a goal.

### R11 — Windows toolchain: USD(MSVC) vs OMNeT++(MinGW) — **HIGH** **(review: new)**
OpenUSD on Windows is effectively built with **MSVC**; OMNeT++ ships a **MinGW** toolchain.
MSVC and MinGW C++ ABIs are incompatible — a MinGW OMNeT++ cannot link an MSVC-built `usd_ms`.
**Mitigation/options (Decision Q9):** (a) build/ship USD with MinGW/clang (may need patches);
(b) provide an MSVC-built OMNeT++ Qtenv+USD plugin on Windows; (c) defer Windows USD support to
a later phase. **Needs a decision before Windows packaging.**

### R12 — Up-axis & units mismatch — **HIGH** **(review: new)**
OSG/INET Z-up & meters; USD default Y-up & cm; glTF mandated Y-up. Unhandled → every model
rotated 90° and scaled 100×, and `.ini` pseudo-loader rotations misapplied.
**Mitigation:** Decision Q8 — author `upAxis=Z`, `metersPerUnit=1`; bake Y-up→Z-up into model
conversion; interpret pseudo-loader rotations in the Z-up frame.

### R13 — Inspector descriptors lost in USD-only build — **MEDIUM** **(review: new)**
`cOsgCanvas` enum `Register_Enum`s and the scene "Fields"/tree browsing live in the OSG
plugin's `osg.msg`; a USD-only build loses them.
**Mitigation:** M2 ports them to a `usd.msg` (or moves them into kernel/qtenv proper).

## B. Open decisions

| # | Decision | Options | Recommendation |
|---|---|---|---|
| **Q1** | Optional OSG-subset migration facade? | (a) build it as a non-product migration vehicle; (b) none — third-party models port by hand | lean **(b)** given "examples re-implemented is fine"; build (a) only if external users need it. **Never ships linked in.** |
| **Q2** | Geospatial v1 fidelity | (a) satellite/sphere only; (b) + static ground terrain | **(b)** — osgEarth removed either way; defer live tiles to M13. |
| **Q3** | OpenUSD distribution | (a) bundle prebuilt; (b) require system/vcpkg/conan; (c) build-from-source script | **(a)+(b)** — prebuilt where possible, documented source build otherwise; optional dependency. |
| **Q4** | Where INET work lands | (a) upstream INET PR; (b) maintained fork; (c) downstream patch set | needs owner input — affects branching & review. |
| **Q5** | Interface rename `IOsgViewer`→`IScene3DViewer` | (a) now; (b) keep name, rename later | **(b)** for the first cut; cosmetic. |
| **Q6** | Text strategy scope | (a) HUD overlay only; (b) HUD + 3D rasterized-quad atlas | **(a)** for annotations; **(b)** only if 3D-anchored labels prove necessary. |
| **Q7** | OpenUSD version pin | latest (25.11/26.x) vs pinned LTS-ish | pin a tested release per OMNeT++ release; verify the `UsdImagingGLEngine` surface against it. |
| **Q8** | Up-axis & units **(review: new)** | author `upAxis=Z, metersPerUnit=1` vs convert to USD defaults | **`upAxis=Z, metersPerUnit=1`** — matches OSG/INET; bake the correction at asset conversion. |
| **Q9** | Windows toolchain **(review: new)** | (a) MinGW/clang USD build; (b) MSVC OMNeT++ on Windows; (c) defer Windows | needs owner input; **(c)** likely acceptable for an early release, **(b)** for parity. |

## C. De-risking spike (do first) **(review: per-platform)**
Before M3, build the minimal OpenUSD imaging config and a ~200-line Qt app rendering a
`UsdStage` (a lit sphere) via `UsdImagingGLEngine` in a `QOpenGLWidget`, with `TestIntersection`
picking and a `QPainter` text overlay — **on both Linux/GL (HgiGL) and macOS/Metal (HgiMetal +
hgiInterop)**. Success: renders + picks correctly, overlay text uncorrupted, backend selection
works, two viewers share an `Hgi` with `AA_ShareOpenGLContexts`. This validates R3 + R11 + R2 —
the items that could invalidate the whole approach — cheaply.

## D. Sources verified during planning
OpenUSD `UsdImagingGLEngine`, Hydra 2 / Storm, HgiGL/HgiMetal/hgiInterop, `TestIntersection`,
`SetLightingState`, UsdGeom/UsdShade/MaterialX, up-axis/`metersPerUnit` schema defaults,
time-and-animated-values, primvar interpolation, OpenUSD 26.03 notes; AOUSD forum threads on
Qt `QOpenGLWidget` interop, GL-state/`QPainter` corruption, and macOS Metal; OpenUSD
`BUILDING.md`/`VERSIONS.md` (MSVC on Windows); cesium-native + `vsgCs`; PROJ C++ API; Adobe USD
file-format plugins; `osgVerse` (osgb→glTF). The review (`docs/07-plan-review.md`) re-verified
the OMNeT++/INET code claims against the real tree.
