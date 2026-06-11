# 03 — Implementation Roadmap

File-level, phased plan. **Invariant: the tree builds and runs at the end of every milestone.**
OSG stays selectable and unbroken until M12. Effort tiers: **[M]** mechanical
(automatable / Sonnet-suitable), **[D]** needs design judgment.

Dependency order:
`M0 → M1 → M2 → M3 → M4 → M5 → M6`  (OMNeT++ core)
`→ M7 → M8 → M9`  (INET)
`→ M10 → M11 → M12`  (geospatial, parity, deprecation);  **M13** is independent/deferrable.

---

## Part I — OMNeT++ core (kernel + Qtenv + build)

### M0 — Neutral public API  **[D]**
Decouple `cOsgCanvas` from `osg::Node*` with **no behavior change**; OSG builds stay identical.
- `include/omnetpp/scene3dnode.h` *(new)* — forward-declare `struct cScene3DNode;` (opaque).
- `include/omnetpp/cosgcanvas.h` — scene root `osg::Node*` → `cScene3DNode*`; retype
  `setScene/getScene`; `using OsgNode = cScene3DNode;` under a compat macro.
- `include/omnetpp/cenvir.h` — add non-pure `refSceneNode/unrefSceneNode(cScene3DNode*)`
  forwarding to the deprecated `refOsgNode/unrefOsgNode`.
- `include/omnetpp/cnullenvir.h`, `src/envir/envirbase.{h,cc}` — matching no-op overrides.
- `src/sim/cosgcanvas.cc` — route ref/unref through the new methods.
- **Validate:** existing `WITH_OSG=yes` build + an OSG sample render unchanged.

### M1 — `WITH_USD` build plumbing  **[M]**
- `configure.in` — OpenUSD detection block (headers + `usd_ms`), sets `USD_CFLAGS/USD_LIBS`,
  `AC_DEFINE(WITH_USD)`/`WITH_USD_GEO`.
- `include/omnetpp/platdep/config.h.in` — `#undef WITH_USD`, `#undef WITH_USD_GEO`.
- `Makefile.inc.in` — `WITH_USD`, `USD_CFLAGS`, `USD_LIBS`; extend `QTENV_LIBS`.
- `src/qtenv/Makefile` — `ifeq ($(WITH_USD),yes)` builds `usd/`.
- `src/qtenv/usd/Makefile` *(new)* — copy of `osg/Makefile` with `oppqtenv-usd`, `USD_LIBS`,
  `QT_NO_KEYWORDS`.
- **Validate:** `WITH_USD=no` (default) builds identically; `WITH_USD=yes` compiles the
  (empty) plugin dir.

### M2 — `oppqtenv-usd` skeleton  **[M]**
- `src/qtenv/usd/usdviewer.{h,cc}` *(new)* — `UsdViewer : IOsgViewer` (all virtuals stubbed),
  `UsdViewerFactory : IOsgViewerFactory` setting the `osgViewerFactory` global on load.
- `src/qtenv/usd/stagecache.{h,cc}` *(new)* — empty `StageCache`.
- `src/qtenv/iosgviewer.cc` — prefer `loadExtensionLibrary("oppqtenv-usd")` under `WITH_USD`.
- **Validate:** load an OSG-using sim with `WITH_USD=yes`; the inspector creates `UsdViewer`,
  shows a placeholder; no crashes.

### M3 — Hydra renders a stage in Qt  **[D]**  *(precede with the GL-interop spike, Risk R3)*
- `usdviewer.cc` — construct `UsdImagingGLEngine` (shared `HdDriver`); implement
  `paintGL`/`resizeGL` (`SetPresentationOutput`/`SetRenderBufferSize`/`SetFraming`/
  `SetCameraState`/`Render`); `HeartBeat` gating on `!IsConverged()`.
- `src/qtenv/usd/cameracontrollers.{h,cc}` *(new)* — `TrackballController` (minimum for M3).
- **Validate:** open a sim with a `cOsgCanvas`; a hardcoded test stage (a sphere) renders;
  mouse-drag orbits.

### M4 — Scene construction + picking  **[D]**
- `stagecache.cc` — `buildStage(cScene3DNode*)`: author a `UsdStage` from the neutral scene.
  (During transition, when fed an `osg::Node` via the optional bridge, walk it with an
  `osg::NodeVisitor`; for native-USD scenes, the handle already references a stage.) Maintain
  a dirty map for incremental updates.
- `usdviewer.cc` — `objectsAt()` via `TestIntersection` + ancestor-path walk through the
  `cPrimObjectBinding` registry; bind objects during `buildStage`.
- **Validate:** `osg-intro` renders its model under USD; left-click selects the right
  `cObject` in the inspector.

### M5 — Camera controllers (all) + viewer hints  **[D]/[M]**
- `cameracontrollers.cc` — add `TerrainController`, `OverviewController` (port
  `cameramanipulators.cc` logic to `GfVec3d`/`GfMatrix4d`).
- `usdviewer.cc` — `applyViewerHints()`: fovy, clear color, manipulator type, z-limits,
  generic viewpoint.
- **Validate:** `osg-indoor` — manipulator menu switches; startup viewpoint correct.

### M6 — Text overlay  **[D]**
- `usdviewer.cc` — GL-state-safe Qt/HUD text pass after `Render`; world-anchor → screen
  projection each frame.
- `include/omnetpp/usdutil.h` *(new)* — `cPrimObjectBinding` (picking) + `cTextLabel3D`
  (overlay anchor/text/color/size); `osgutil.h` redirects here under `WITH_USD`.
- **Validate:** node labels render correctly; no `QPainter` corruption.

---

## Part II — INET (`samples/inet-4.6.0/src/inet/visualizer/`)

Strategy D3: a parallel `visualizer/usd/` tree subclassing the same `base/` classes, sharing
a `UsdUtils`/`UsdScene` chokepoint. Tier the ~84 files:
- **Tier-1 infrastructure (deep rewrites, ~7):** `UsdUtils`, `UsdScene`,
  `NetworkNodeUsdVisualization`, `MediumUsdVisualizer`, `SceneUsdVisualizerBase`,
  `PhysicalEnvironmentUsdVisualizer`, `MobilityUsdVisualizer`.
- **Tier-2 moderate (~15):** link/path bases + annotation visualizers.
- **Tier-3 thin (~40+):** mostly-mechanical ports + `.ned` package files.

### M7 — INET helper layer + node visualization + assets  **[D]**
- `.oppfeatures` — add `VisualizationUsd` (`-DINET_WITH_VISUALIZATIONUSD`,
  `nedPackages=inet.visualizer.usd`) and `VisualizationUsdShowcases`.
- `src/inet/visualizer/usd/{util,scene,base,…}/` *(new dirs)*.
- `usd/util/UsdUtils.{h,cc}` — USD-typed analogues of `OsgUtils`: `createLineCurve` →
  `UsdGeomBasisCurves`, `createAnnulusMesh`/`createCircleCurve`/`createPolygonMesh`,
  `createTextLabel` (→ overlay handle), `createBillboard` (→ camera-faced handle),
  `createXform`, `createMaterial`/`displayColor`, `createTexture` (`UsdUVTexture`),
  `resolveImageResource` (port intact). `UsdLineNode` struct (SdfPath to a curve;
  `setStart/setEnd` re-author `points`).
- `usd/util/UsdScene.{h,cc}` — `TopLevelUsdScene` / `SimulationUsdScene` (prims at
  `/World` and `/World/Simulation`); `getSimulationScene(cModule*)`.
- `usd/scene/NetworkNodeUsdVisualization.{h,cc}` — holds an `SdfPath` to a `UsdGeomXform`
  (no longer *is-a* transform node); `addAnnotation/updateAnnotationPositions`.
- `usd/scene/{SceneUsdVisualizerBase,SceneUsdVisualizer,NetworkNodeUsdVisualizer}`.
- **Asset pipeline:** offline `.osgb → glTF → .usd` for stock models (`cow`, `dumptruck`,
  `boxman`, `office`, `satellite`, `dishlow`, `glider`) using `osgVerse` (osgb→glTF) +
  Adobe `usdGLTF` (glTF→USD); `UsdUtils::loadModel()` parses the osgDB pseudo-loader
  transform-URI (`name.ext.scale.rot.trans`) and applies it as `UsdGeomXform` ops; icons via
  `UsdUVTexture`.
- **Validate:** `showcases/visualizer/osg/networknode` scenario — nodes appear (3D models &
  icon billboards) under USD.

### M8 — INET visualizer port (the bulk)  **mostly [M], some [D]**
- **Annotation [M]:** `Info`, `Statistic`, `Radio`, `EnergyStorage`, `Queue`, `GateSchedule`
  → overlay text via `UsdUtils`.
- **Mobility [M]:** `MobilityUsdVisualizer` — `UsdGeomBasisCurves` trail, per-frame `points`
  re-author (capped length).
- **Link/Path [M]:** `LinkUsdVisualizerBase`/`PathUsdVisualizerBase` + concretes
  (`DataLink`, `Ieee80211`, `InterfaceTable`, `LinkBreak`, `PacketDrop`, `NetworkRoute`,
  `RoutingTable`, `TransportConnection`, `TransportRoute`, `PacketFlow`).
- **Environment [M]:** `PhysicalEnvironmentUsdVisualizer` — `UsdGeomCube/Sphere/Mesh`;
  wireframe via display opacity/`purpose`.
- **Validate per category** against the matching `showcases/visualizer/osg/*` scenario.

### M9 — Signal & scene shaders  **[D]**
- `MediumUsdVisualizer` — rings as `UsdGeomMesh` annulus (per-frame `points`), spheres as
  `UsdGeomSphere` (per-frame `radius`/`displayOpacity`).
- Wave-fade & floor-fade effects: **primary** = MaterialX node graph (`ND_length/divide/
  power/cos/multiply/add`) with shader inputs authored as time samples; **fallback** =
  CPU-computed per-vertex `displayOpacity` (loses the in-ring wave oscillation — documented
  gap). `ImageStream` animated icons → static icon (Phase 1) or UV-offset atlas (later).
- **Validate:** `showcases/visualizer/osg/medium` — propagation rings animate with correct
  radii/fade.

---

## Part III — Geospatial, parity, deprecation

### M10 — Geospatial Phase 1 (PROJ + static earth/terrain)  **[D]**
Detail in [`docs/05-geospatial.md`](docs/05-geospatial.md). In brief:
- Integrate **PROJ 9.x**; `UsdGeographicCoordinateSystem` replaces
  `OsgGeographicCoordinateSystem` (EPSG:4326 ↔ 4978 + ENU matrix in plain C++).
- `UsdGeoAnchor` replaces `osgEarth::GeoTransform` (`setPosition(lat,lon,alt)` → ECEF →
  scene-local → `xformOp:translate`).
- `SceneUsdGeoVisualizer` replaces `SceneOsgEarthVisualizer`; static earth sphere
  (satellite scenarios) / static terrain mesh (ground scenarios); `UsdEarthGround` replaces
  `OsgEarthGround` (elevation via CPU ray cast on the terrain mesh).
- Annotation equivalents: `LabelNode`→overlay, `CircleNode`→geo-anchored
  `UsdGeomBasisCurves`, `FeatureNode/LineString`→curves in ECEF, `SkyNode`→static skybox.
- Behind `WITH_USD_GEO`; documented fidelity gaps (no terrain drape, no live tiles, no
  building extrusion).

### M11 — Sample port + parity validation  **[M]/[D]**
- Port `osg-intro`, `osg-indoor`, `osg-earth`, `osg-satellites` → USD equivalents
  (`usd-intro`, …) using the same model assets (converted) and the geospatial layer.
- Side-by-side visual regression vs the OSG output for the same scenarios; tick the
  `DEVELOPMENT_PLAN.md` §5 parity checklist.

### M12 — OSG deprecation  **[M]**
- `configure.in` deprecation warning for `WITH_OSG`/`WITH_OSGEARTH`.
- Migration guide for custom C++ models (osg::Node authoring → USD helper, or the temporary
  bridge); update `DummyViewer` message and docs.
- Schedule removal of `src/qtenv/osg/`, `osgutil.h`, INET `visualizer/osg/`, and the OSG
  build machinery in a subsequent major release.

### M13 — Tiled terrain (optional, later)  **[D]**
Cesium-native 3D Tiles → glTF → USD streaming for live high-res terrain/imagery; terrain
elevation via cesium-native; `.earth` config → 3D-Tiles URL. Independent of M0–M12.

---

## Sequencing & parallelism for agent execution
- **[M] milestones/steps** fan out cleanly to Sonnet subagents — one per file/category
  (Tier-3 INET ports, build plumbing, asset-conversion runs, parity scaffolding).
- **[D] steps** (M0, M3, M4, M6, M7, M9, M10) need large-model design/review; Sonnet handles
  the surrounding edits.
- M7's `UsdUtils`/`UsdScene` chokepoint must land before M8 can be parallelized across
  categories. M3's GL-interop spike (Risk R3) must succeed before committing to the viewer.
