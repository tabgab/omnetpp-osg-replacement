# 03 — Implementation Roadmap

File-level, phased plan. **Invariant: the tree builds and runs at the end of every milestone.**
OSG/osgEarth stay selectable **as a transitional build convenience** until M12, when both are
**removed**. Effort tiers: **[M]** mechanical (automatable / Sonnet-suitable), **[D]** needs
design judgment.

Dependency order:
`M0 → M1 → M2 → M3 → M4 → M5 → M6`  (OMNeT++ core)
`→ M7 → M8 → M9`  (INET)
`→ M10 → M11 → M12`  (geospatial, parity, removal);  **M13** is an independent later enhancement.

---

## Part I — OMNeT++ core (kernel + Qtenv + build)

### M0 — Neutral public API  **[D]**
Generalize `cOsgCanvas` away from a hard `osg::Node*` with **no behavior change**; OSG builds
stay identical because the handle aliases `osg::Node` under `WITH_OSG`.
- `include/omnetpp/scene3dnode.h` *(new)* — `cScene3DNode = osg::Node` when `WITH_OSG`, else an
  opaque forward-declared type. **(Alias direction matters — see `docs/02` §2; the reverse
  would not compile INET/sample/qtenv `osg::Node` consumers.)**
- `include/omnetpp/cosgcanvas.h` — scene root `osg::Node*` → `cScene3DNode*`; retype
  `setScene/getScene` (a no-op under `WITH_OSG`).
- `include/omnetpp/cenvir.h` — add non-pure `refSceneNode/unrefSceneNode(cScene3DNode*)`
  forwarding to the deprecated `refOsgNode/unrefOsgNode`.
- `include/omnetpp/cnullenvir.h`, `src/envir/envirbase.{h,cc}` — matching no-op overrides.
- `src/sim/cosgcanvas.cc` — route ref/unref through the new methods.
- **Validate:** existing `WITH_OSG=yes` build + an OSG sample render unchanged; headless
  Cmdenv (no viewer) still constructs `cOsgCanvas` fine.

### M1 — `WITH_USD` build plumbing  **[M]**
- `configure.in` — OpenUSD detection (headers + `usd_ms`), `USD_CFLAGS/USD_LIBS`,
  `AC_DEFINE(WITH_USD)`/`WITH_USD_GEO`; PROJ probe; **HgiMetal on macOS**.
- `include/omnetpp/platdep/config.h.in` — `#undef WITH_USD`, `#undef WITH_USD_GEO`.
- `Makefile.inc.in` — `WITH_USD`, `USD_CFLAGS`, `USD_LIBS`; extend `QTENV_LIBS`.
- `src/qtenv/Makefile` — `ifeq ($(WITH_USD),yes)` builds `usd/`.
- `src/qtenv/usd/Makefile` *(new)* — `oppqtenv-usd`, `USD_LIBS`, `hgiMetal` on macOS,
  `QT_NO_KEYWORDS`.
- **Validate:** `WITH_USD=no` (default) builds identically; `WITH_USD=yes` compiles the
  (empty) plugin dir on Linux **and macOS**.

### M2 — `oppqtenv-usd` skeleton + inspector descriptors  **[M]**
- `src/qtenv/usd/usdviewer.{h,cc}` *(new)* — `UsdViewer : IOsgViewer` (stubs),
  `UsdViewerFactory : IOsgViewerFactory` setting the `osgViewerFactory` global on load.
- `src/qtenv/usd/stagecache.{h,cc}` *(new)* — empty `StageCache`.
- `src/qtenv/usd/usd.msg` *(new)* — port the `cOsgCanvas` enum/struct `Register_Enum`s and
  scene "Fields"/tree-browsing descriptors that currently live in the OSG plugin's `osg.msg`,
  so the inspector works in a USD-only build. *(Review fix — these vanish otherwise.)*
- `src/qtenv/iosgviewer.cc` — prefer `loadExtensionLibrary("oppqtenv-usd")` under `WITH_USD`.
- **Validate:** load an OSG-using sim with `WITH_USD=yes`; inspector creates `UsdViewer`, shows
  a placeholder, Fields tab still works; no crashes.

### M3 — Hydra renders a stage in Qt  **[D]**  *(precede with the GL/Metal-interop spike, Risk R3)*
- Qtenv startup — set `QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts)` before the
  `QApplication` (needed to share one `Hgi` across inspectors). *(Review fix.)*
- `usdviewer.cc` — construct `UsdImagingGLEngine` with a shared `Hgi` (**HgiGL** on Linux/
  Windows, **HgiMetal + hgiInterop** on macOS — Decision D6); `paintGL`/`resizeGL`
  (`SetPresentationOutput`/`SetRenderBufferSize`/`SetFraming`/`SetCameraState`/
  **`SetLightingState`** /`Render`); own 30 fps heartbeat gating on `!IsConverged()`.
- `src/qtenv/usd/cameracontrollers.{h,cc}` *(new)* — `TrackballController` (minimum for M3).
- Author the test stage with `upAxis=Z`, `metersPerUnit=1` (Decision Q8).
- **Validate on Linux/GL and macOS/Metal:** a hardcoded test stage (a lit sphere) renders;
  mouse-drag orbits.

### M4 — Scene construction + picking  **[D]**
- `stagecache.cc` — `buildStage(cScene3DNode*)`: author a `UsdStage` from the neutral scene
  (Z-up, meters); maintain a dirty map for incremental updates.
- `usdviewer.cc` — `objectsAt()` via `TestIntersection(resolveDeep)` + ancestor-path walk
  through the `cPrimObjectBinding` registry; bind objects during `buildStage`.
- **Validate with a native-USD test sample** authored directly in USD (a small new sample,
  *not* via any OSG bridge — the bridge is not part of the product). Left-click selects the
  right `cObject`. *(Review fix: M4–M6 must not depend on an OSG→USD bridge.)*

### M5 — Camera controllers (all) + viewer hints  **[D]/[M]**
- `cameracontrollers.cc` — add `TerrainController`, `OverviewController` (port
  `cameramanipulators.cc` logic to `GfVec3d`/`GfMatrix4d`).
- `usdviewer.cc` — `applyViewerHints()`: fovy, clear color, manipulator type, z-limits,
  generic viewpoint.
- **Validate** with the native-USD test sample: manipulator menu switches; startup viewpoint
  correct.

### M6 — Text overlay  **[D]**
- `usdviewer.cc` — GL-state-safe Qt/HUD text pass after `Render`; world-anchor → screen
  projection each frame.
- `include/omnetpp/usdutil.h` *(new)* — `cPrimObjectBinding` (picking) + `cTextLabel3D`
  (overlay anchor/text/color/size); `osgutil.h` redirects here under `WITH_USD`.
- **Validate:** labels render correctly; no `QPainter` corruption on either backend.

---

## Part II — INET (`samples/inet-4.6.0/src/inet/visualizer/`)

Strategy D3: a parallel `visualizer/usd/` tree subclassing the same `base/` classes, sharing a
`UsdUtils`/`UsdScene` chokepoint. The `osg/` tree is ~90 files (the inventory's "~84" was an
undercount; verified). The OSG include surface is small (~88 include lines across ~33 files),
so the include churn is modest; the real work is in ~7 infra files. Tiers:
- **Tier-1 infrastructure (deep, ~7):** `UsdUtils`, `UsdScene`, `NetworkNodeUsdVisualization`,
  `MediumUsdVisualizer`, `SceneUsdVisualizerBase`, `PhysicalEnvironmentUsdVisualizer`,
  `MobilityUsdVisualizer`.
- **Tier-2 moderate (~15):** link/path bases + annotation visualizers.
- **Tier-3 thin (~40+):** near-mechanical ports + `.ned` files.

### M7 — INET helper layer + node visualization + assets  **[D]**
- `.oppfeatures` — add `VisualizationUsd` (`-DINET_WITH_VISUALIZATIONUSD`,
  `nedPackages=inet.visualizer.usd`) and `VisualizationUsdShowcases`.
- `src/inet/visualizer/usd/{util,scene,base,…}/` *(new dirs)*.
- `usd/util/UsdUtils.{h,cc}` — USD-typed analogues of `OsgUtils` (`createLineCurve` →
  `UsdGeomBasisCurves`, `createAnnulusMesh`/`createCircleCurve`/`createPolygonMesh`,
  `createTextLabel` → overlay handle, `createBillboard` → camera-faced handle, `createXform`,
  `createMaterial`/`displayColor`, `createTexture` (`UsdUVTexture`), `resolveImageResource`
  ported intact). `UsdLineNode` (SdfPath to a curve; `setStart/setEnd` re-author `points`).
- `usd/util/UsdScene.{h,cc}` — `TopLevelUsdScene`/`SimulationUsdScene` (`/World`,
  `/World/Simulation`); `getSimulationScene(cModule*)`.
- `usd/scene/NetworkNodeUsdVisualization.{h,cc}` — holds an `SdfPath` to a `UsdGeomXform`;
  `addAnnotation/updateAnnotationPositions`.
- `usd/scene/{SceneUsdVisualizerBase,SceneUsdVisualizer,NetworkNodeUsdVisualizer}`.
- **Asset pipeline (with up-axis/units handling — Decision Q8):** offline `.osgb → glTF →
  .usd` for stock models (`cow`, `dumptruck`, `boxman`, `office`, `satellite`, `dishlow`,
  `glider`) via `osgVerse` (osgb→glTF) + Adobe `usdGLTF` (glTF→USD), **baking the Y-up→Z-up
  +90°X correction** so models sit right in a Z-up/meters scene; `UsdUtils::loadModel()`
  parses the osgDB pseudo-loader transform-URI and applies it **in the Z-up frame**; icons via
  `UsdUVTexture`.
- **Validate:** `showcases/visualizer/osg/networknode` (via NED alias, see M8) — nodes appear
  (3D models & icon billboards), correctly oriented and scaled.

### M8 — INET visualizer port + NED aliases  **mostly [M], some [D]**
- **NED aliases (review fix):** add deprecated alias modules so existing `.ini`/showcases that
  name `IntegratedOsgVisualizer`, `SceneOsgVisualizer`, `SceneOsgEarthVisualizer`, etc. resolve
  to the USD implementations (or keep both names during transition).
- **Annotation [M]:** `Info`, `Statistic`, `Radio`, `EnergyStorage`, `Queue`, `GateSchedule`
  → overlay text.
- **Mobility [M]:** `UsdGeomBasisCurves` trail, per-frame `points` re-author (capped).
- **Link/Path [M]:** `LinkUsdVisualizerBase`/`PathUsdVisualizerBase` + concretes (`DataLink`,
  `Ieee80211`, `InterfaceTable`, `LinkBreak`, `PacketDrop`, `NetworkRoute`, `RoutingTable`,
  `TransportConnection`, `TransportRoute`, `PacketFlow`).
- **Environment [M]:** `PhysicalEnvironmentUsdVisualizer` — `UsdGeomCube/Sphere/Mesh`;
  wireframe via Hydra draw mode / edge curves.
- **Validate** each category against the matching `showcases/visualizer/osg/*` scenario.

### M9 — Signal & scene shaders + lighting  **[D]**
- `MediumUsdVisualizer` — rings as `UsdGeomMesh` annulus (per-frame `points`), spheres as
  `UsdGeomSphere` (per-frame `radius`/`displayOpacity`).
- Wave-fade & floor-fade: **primary** = MaterialX node graph (`ND_length/divide/power/cos/
  multiply/add`) with shader inputs authored as time samples; **fallback** = CPU per-vertex
  `displayOpacity` (loses the in-ring wave oscillation — documented gap). `ImageStream` icons →
  static icon (now) or UV-offset atlas (later).
- Confirm scene lighting reads correctly for shaded materials on both Hgi backends.
- **Validate:** `showcases/visualizer/osg/medium` — propagation rings animate with correct
  radii/fade.

---

## Part III — Geospatial (osgEarth removal), parity, removal

### M10 — Geospatial replacement (shipped, not optional)  **[D]**
Detail in [`docs/05-geospatial.md`](docs/05-geospatial.md). osgEarth is **removed**; this is
its native USD replacement:
- Integrate **PROJ 9.x**; `UsdGeographicCoordinateSystem` replaces `OsgGeographicCoordinateSystem`
  (EPSG:4326 ↔ 4978 + ENU matrix in plain C++).
- `UsdGeoAnchor` replaces `osgEarth::GeoTransform`.
- `SceneUsdGeoVisualizer` replaces `SceneOsgEarthVisualizer`; native USD earth sphere
  (satellite scenarios) / terrain mesh (ground scenarios); `UsdEarthGround` replaces
  `OsgEarthGround` (elevation via CPU ray cast). All osgEarth `#if defined(WITH_OSGEARTH)`
  code in INET `common/`/`environment/` is reimplemented behind `WITH_USD_GEO`.
- Annotation equivalents: `LabelNode`→overlay, `CircleNode`→geo-anchored `UsdGeomBasisCurves`,
  `FeatureNode/LineString`→curves in ECEF, `SkyNode`→static skybox.
- Documented fidelity gaps (no terrain drape, no live tiles, no building extrusion) — accepted.

### M11 — Sample re-implementation + parity validation  **[M]/[D]**
- **Re-implement** `osg-intro`, `osg-indoor`, `osg-earth`, `osg-satellites` as USD samples
  (`usd-intro`, …) using the converted assets and the geospatial layer; re-implementation is
  expected and in scope (per the project goal).
- Side-by-side visual regression vs OSG for the same scenarios; tick the `DEVELOPMENT_PLAN.md`
  §5 parity checklist.

### M12 — Remove OSG & osgEarth  **[M]**
- **Delete** `src/qtenv/osg/`, `include/omnetpp/osgutil.h`, INET `src/inet/visualizer/osg/`,
  the osgEarth code in INET `common/`/`environment/`, and all `WITH_OSG`/`WITH_OSGEARTH`
  detection/flags/link fragments. Drop the `cScene3DNode = osg::Node` alias; the handle becomes
  the USD stage handle.
- Verify **no `osg::`/`osgEarth::` symbol remains** (grep gate in CI).
- Migration guide for custom C++ models (osg::Node authoring → USD helper, or the optional
  migration facade if built — Q1); update `DummyViewer` message and docs.

### M13 — Tiled terrain (optional later enhancement)  **[D]**
Cesium-native 3D Tiles → glTF → USD streaming for live high-res terrain/imagery; elevation via
cesium-native; config via a 3D-Tiles URL. Independent of M0–M12; restores osgEarth-class
terrain fidelity on the future-proof substrate. **Not required to remove osgEarth.**

---

## Sequencing & parallelism for agent execution
- **[M] steps** fan out to Sonnet subagents (Tier-3 INET ports, build plumbing, asset
  conversion, NED aliases, parity scaffolding).
- **[D] steps** (M0, M3, M4, M6, M7, M9, M10) need large-model design/review; Sonnet does the
  surrounding edits.
- M7's `UsdUtils`/`UsdScene` chokepoint lands before M8 parallelizes across categories. M3's
  **GL/Metal-interop spike (Risk R3) must succeed on both Linux and macOS** before committing
  to the viewer.
