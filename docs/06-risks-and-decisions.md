# 06 — Risk Register & Open Decisions

## A. Risks & mitigations

### R1 — Billboards (`AutoTransform`) have no USD equivalent — **HIGH**
OSG orients/scales billboards toward the camera transparently each render frame
(`ROTATE_TO_SCREEN`, `autoScaleToScreen`). USD has no camera-facing constraint.
**Mitigation:** text labels → viewer HUD overlay (cleanest, naturally screen-scaled). Icon
billboards → per-tick `xformOp:orient`/scale computed in `refreshDisplay()` from a
viewer-provided camera direction. Orientation snaps at simulation-tick granularity, not per
render frame.
**Residual gap:** between ticks, billboard orientation is "stale" during fast camera
fly-throughs. Acceptable for a sim visualization tool.

### R2 — Picking fidelity for thin geometry — **MEDIUM**
`TestIntersection` uses an ID render pass; thin lines/arrowheads (INET links) hit-test worse
than `osgUtil::LineSegmentIntersector`.
**Mitigation:** `resolveDeep` pick mode; for lines, add invisible thick pick-proxy meshes
(cylinders) alongside the visible `UsdGeomBasisCurves` — a standard USD-DCC pattern.

### R3 — Qt + Hydra GL-context interop — **HIGH (top technical risk)**
`UsdImagingGLEngine`/HgiGL and `QOpenGLWidget` both manage GL state. Known issues: Hydra
leaves pixel-transfer state that corrupts `QPainter` overlays; HgiGL needs OpenGL ≥ 4.5;
sharing `Hgi`/`HdDriver` across multiple inspector viewers.
**Mitigation:** **a throwaway Qt + `UsdImagingGLEngine` spike before committing to M3.**
Overlay text via the QImage→texture path (verified workaround). Check the GL version at
`UsdViewer` construction; fall back to `DummyViewer` if < 4.5. Create one global `HdDriver`
on first viewer and share it. `QT_NO_KEYWORDS` in the plugin build (mandatory).

### R4 — Per-frame attribute re-author performance — **MEDIUM**
USD writes whole arrays (no sparse override). INET mutates trail/ring vertex arrays and sphere
radii every tick.
**Mitigation:** author at `UsdTimeCode::Default()` and **overwrite** (Default is a single
value — no time-sample accumulation, no unbounded memory). Cap trail length; consider
`UsdGeomPointInstancer` for many moving entities. OpenUSD 26.x added targeted mesh
invalidation (points/transforms/extents) that helps. **Profile at M8/M9** (target 30 fps with
~50–100 moving nodes).

### R5 — Inline GLSL shaders — **MEDIUM**
INET's signal wave-fade and scene-floor-fade use `osg::Program`/`Shader`/`Uniform`. Hydra
Storm shading is MaterialX/`.glslfx`; custom GLSL injection is sparsely documented.
**Mitigation:** primary = MaterialX node graph (the wave-fade formula decomposes into
standard `ND_*` nodes; uniforms → time-sampled shader inputs). Fallback = CPU per-vertex
`displayOpacity`.
**Residual gap:** the CPU fallback loses the *in-ring* cosine wave oscillation (becomes a
smooth radial fade). Visible but acceptable degradation; MaterialX port restores it later.

### R6 — `osg::ImageStream` animated textures — **LOW**
Used for blinking signal icons. USD has no animated texture.
**Mitigation:** static first-frame icon (Phase 1); UV-offset atlas animation later.

### R7 — osgEarth has no 1:1 path — **HIGH (scope, not technical)**
See [`docs/05-geospatial.md`](docs/05-geospatial.md). Phase 1 omits live tiles, terrain
draping, declutter, feature symbology, and building extrusion.
**Mitigation:** PROJ + static earth/terrain covers the satellite case well and the ground
case acceptably; live terrain is the deferred Phase 3 (cesium-native). **Sim behavior is
never affected — only visuals.**

### R8 — INET port scale — **HIGH (organizational)**
~84 files; ~250 include sites; logical changes for text/shaders/billboards.
**Mitigation:** parallel-tree strategy concentrates real work in ~7 infra files; ~40 are
near-mechanical (fan out to Sonnet agents). OSG stays selectable so the port can land
incrementally. Decide early whether INET work goes **upstream** or into a fork (Decision Q4).

### R9 — OpenUSD build/packaging size & availability — **MEDIUM**
Monolithic `usd_ms` is larger than the OSG libs; building USD is non-trivial.
**Mitigation:** minimal imaging config (~30 MB); ship as an **optional** dependency like OSG
was; lean on prebuilt USD from vcpkg/conan/Homebrew where available; document the build.

### R10 — Third-party model breakage — **MEDIUM**
Custom C++ models that built `osg::Node` graphs and called `setScene(osg::Node*)` break when
the scene-root type changes.
**Mitigation:** migration guide; the optional temporary OSG→USD bridge
(`liboppscene-osgcompat`) for models that can't be ported immediately. Communicate clearly in
release notes; this is a deliberate, one-time break to kill OSG.

## B. Open decisions (need sign-off before/early in implementation)

| # | Decision | Options | Recommendation |
|---|---|---|---|
| **Q1** | Permanent OSG-compat shim, or native USD with only a temporary bridge? | (a) permanent facade; (b) native + temporary bridge | **(b)** — a permanent shim re-creates OSG's design and rot. |
| **Q2** | Geospatial Phase-1 fidelity target | (a) satellite/sphere only; (b) + static ground terrain; (c) hold for cesium-native | **(b)** — covers both INET scenarios acceptably; defer live tiles. |
| **Q3** | OpenUSD distribution | (a) bundle prebuilt; (b) require system/vcpkg/conan; (c) build-from-source script | **(a)+(b)** — prebuilt where possible, documented source build otherwise; optional dependency. |
| **Q4** | Where INET work lands | (a) upstream INET PR; (b) maintained fork; (c) downstream patch set | needs owner input — affects branching & review. |
| **Q5** | Interface rename `IOsgViewer`→`IScene3DViewer` | (a) rename now; (b) keep name, rename later | **(b)** — keep the name for the first cut; cosmetic rename is low-value churn early. |
| **Q6** | Text strategy scope | (a) HUD overlay only; (b) HUD + 3D rasterized-quad atlas for in-scene labels | **(a)** for all annotations; **(b)** only if 3D-anchored packet-name labels prove necessary. |
| **Q7** | OpenUSD version pin | track latest (25.11/26.x) vs pin an LTS-ish release | pin a tested release per OMNeT++ release; verify the `UsdImagingGLEngine` surface against it. |

## C. De-risking spike (do first)
Before M3, build the minimal OpenUSD imaging config and a ~200-line Qt app that renders a
`UsdStage` (a sphere) via `UsdImagingGLEngine` in a `QOpenGLWidget`, with `TestIntersection`
picking and a `QPainter` text overlay. Success criteria: renders correctly, picks correctly,
overlay text is not corrupted, GL ≥ 4.5 detection works, two viewers share an `Hgi`. This
validates Risks R2 + R3 — the two that could invalidate the whole approach — cheaply.

## D. Sources verified during planning
OpenUSD `UsdImagingGLEngine`, Hydra 2 / Storm, `TestIntersection`, UsdGeom/UsdShade/MaterialX,
time-and-animated-values, primvar interpolation, OpenUSD 26.03 release notes; AOUSD forum
threads on Qt `QOpenGLWidget` interop and GL-state/`QPainter` corruption; OpenUSD `BUILDING.md`;
cesium-native + `vsgCs`; PROJ C++ API; Adobe USD file-format plugins; `osgVerse` (osgb→glTF).
(Full URLs were captured in the design-agent outputs that fed this plan.)
