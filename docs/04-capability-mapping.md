# 04 — OSG → OpenUSD Capability Mapping

Every OSG / osgText / osgDB construct used across OMNeT++, INET, and the samples, mapped to
its OpenUSD / Hydra equivalent or chosen fallback. **Clean?** = does a faithful 1:1 mapping
exist. The "no clean mapping" rows are the design risks tracked in
[`docs/06-risks-and-decisions.md`](docs/06-risks-and-decisions.md). osgEarth specifics are in
[`docs/05-geospatial.md`](docs/05-geospatial.md).

> USD API names should be confirmed against the OpenUSD version pinned at build time; they
> are stable across 25.11/26.x for the surface used here.

## Scene-graph structure

| OSG | OpenUSD / Hydra | Clean? | Notes |
|---|---|---|---|
| `osg::Group`, `osg::Node` | `UsdGeomXform` / generic `UsdPrim` scope | ✅ | container = Xform or plain prim |
| `osg::Geode` (drawable leaf) | the gprim itself (`UsdGeomMesh`/…) | ✅ | USD has no separate "geode" |
| `osg::PositionAttitudeTransform` | `UsdGeomXform` + `xformOp:translate`/`orient`/`scale` | ✅ | |
| `osg::MatrixTransform` | `UsdGeomXformable::MakeMatrixXform()` | ✅ | |
| `osg::Switch` / `setNodeMask` (show/hide) | `UsdGeomImageable` visibility (`MakeInvisible`) | ✅ | |
| `osg::LOD` | `UsdGeomXform` + manual switch, or Hydra LOD | ⚠️ | not used heavily; manual |
| `osg::ref_ptr<T>` / `osg::Referenced` | `TfRefPtr`/`UsdStageRefPtr`; prims are stage-owned | ✅ | ownership model differs but covered |
| `osg::NodeVisitor` / `FindNodesVisitor` | `UsdPrimRange` traversal / `Usd...` API | ✅ | replace visitor with range iteration |
| `osg::NodePath` (picking ancestry) | `SdfPath` + `GetParentPath()` walk | ✅ | |

## Geometry & drawables

| OSG | OpenUSD | Clean? | Notes |
|---|---|---|---|
| `Geometry` + `Vec3Array` + `DrawArrays(LINE_STRIP)` | `UsdGeomBasisCurves` (`type=linear`) | ✅ | per-frame `points` re-author |
| `DrawArrays(LINE_LOOP)` (circle) | `UsdGeomBasisCurves` (`wrap=periodic`) | ✅ | |
| `DrawArrays(TRIANGLE_STRIP)` (annulus) | `UsdGeomMesh` (triangulated) | ✅ | build ring verts; per-frame `points` |
| `DrawArrays(POLYGON/QUADS)` | `UsdGeomMesh` (faceVertexCounts/Indices) | ✅ | quads → tris if needed |
| `ShapeDrawable(osg::Sphere)` | `UsdGeomSphere` (`radius`) | ✅ | per-frame `radius` for expanding signals |
| `ShapeDrawable(osg::Box)` | `UsdGeomCube` + scale | ✅ | unit cube scaled by Xform |
| `ShapeDrawable(osg::Cone)` | `UsdGeomCone` | ✅ | |
| `createTexturedQuadGeometry` (icon) | `UsdGeomMesh` quad + `UsdUVTexture` | ✅ geometry | facing = billboard problem (below) |
| `dirtyBound()`/`dirtyDisplayList()` | re-`Set()` the attr → Hydra dirty-bit sync | ✅ | see Risk R6 (perf) |

## Transforms that need the camera

| OSG | OpenUSD | Clean? | Notes |
|---|---|---|---|
| `AutoTransform` `ROTATE_TO_SCREEN` (billboard) | **none** | ❌ | per-tick C++ orient (`xformOp:orient` toward camera) **or** viewer HUD |
| `AutoTransform` `ROTATE_TO_AXIS` | **none** | ❌ | per-tick C++ orient about axis |
| `AutoTransform` `autoScaleToScreen` | **none** | ❌ | per-tick scale from camera distance, viewer-side |

USD has no camera-facing constraint. Text labels move to the viewer HUD (cleanest); icon
billboards get a per-tick `xformOp:orient`/scale computed from a viewer-provided camera
direction. Orientation updates at simulation-tick granularity, not per render frame —
acceptable for a sim visualization (Risk R1).

## Appearance / state

| OSG | OpenUSD | Clean? | Notes |
|---|---|---|---|
| `Material` ambient/diffuse/alpha | `UsdPreviewSurface` **or** `displayColor`/`displayOpacity` primvars | ✅ | `displayOpacity` is time-sampleable for fades |
| `StateSet` `TRANSPARENT_BIN`/`OPAQUE_BIN` | opacity-driven ordering in Storm; `purpose` | ⚠️ | automatic; rarely needs explicit control |
| `Texture2D` (+`REPEAT`) from `readImageFile` | `UsdShade` + `UsdUVTexture` (`wrapS/T=repeat`) | ✅ | |
| `LineWidth` | `UsdGeomBasisCurves` `widths` primvar | ✅ | |
| `LineStipple` (dashed/dotted) | **partial** | ⚠️ | no native stipple → dash as segment gaps, or MaterialX cutout |
| `CullFace` | mesh orientation / `doubleSided` | ✅ | |
| `PolygonMode` (wireframe) | Hydra `wireframe`/`wireOnSurface` draw mode, or edge curves | ⚠️ | per-prim wireframe needs draw-mode control |
| `PolygonOffset` (z-fight) | small depth bias / slight position offset | ⚠️ | author a tiny offset on coplanar geo |
| `Depth` `setWriteMask(false)` (overlay) | render order via opacity / `purpose` | ⚠️ | overlays drawn last |
| `Program`+`Shader`+`Uniform` (inline GLSL) | **partial** — MaterialX node graph, or `.glslfx` `info:glslfx:sourceAsset` | ⚠️/❌ | wave-fade port or CPU fallback (Risk R5) |

## Text

| OSG | OpenUSD | Clean? | Notes |
|---|---|---|---|
| `osgText::Text` (labels, stats, packet names) | **none** | ❌ | viewer Qt/HUD overlay (preferred) or rasterized-quad atlas |
| `osgText::Font` (`getDefaultFont`) | Qt font for HUD/atlas rasterization | ✅ | font handled viewer-side |

## File loading / assets

| OSG | OpenUSD | Clean? | Notes |
|---|---|---|---|
| `osgDB::readNodeFile(".osgb")` | `UsdStage::Open(".usd")` / `UsdReferences` | ✅ after conversion | offline `.osgb`→glTF→`.usd` |
| `osgDB::readImageFile(".png/.gif/.jpg")` | `SdfAssetPath` → `UsdUVTexture` | ✅ | `resolveImageResource` ports intact |
| osgDB pseudo-loader URI (`x.osgb.2.scale.…`) | parse + apply as `UsdGeomXform` ops at load | ⚠️ | keep syntax; swap base extension to `.usd` |
| `osg::ImageStream` (animated icon) | **none** | ❌ | static frame (Phase 1) or UV-offset atlas (later) |
| `osgUtil::Optimizer` / `DatabasePager` | Hydra handles scene optimization/streaming | ✅ | not needed explicitly |

## Viewer / camera / interaction  *(OMNeT++ core only)*

| OSG | OpenUSD / viewer-side | Clean? | Notes |
|---|---|---|---|
| `osgViewer::CompositeViewer` / `View` | `UsdImagingGLEngine` (shared `HdDriver`) | ✅ | |
| `GraphicsWindowEmbedded` (OSG↔Qt GL) | `SetPresentationOutput(defaultFramebufferObject())` | ✅ | no bridge class needed |
| `osg::Camera` perspective/clear | `GfCamera`/`SetCameraState` + `RenderParams::clearColor` | ✅ | |
| `osgGA::TrackballManipulator` | C++ `TrackballController` | ✅ | reimplement (USD has none) |
| `osgGA::TerrainManipulator` | C++ `TerrainController` | ✅ | |
| custom `OverviewManipulator` | C++ `OverviewController` | ✅ | port logic verbatim |
| `osgEarth::EarthManipulator` | C++ earth-orbit controller | ⚠️ | geospatial phase |
| `osgGA::EventQueue` | direct Qt event handling | ✅ | |
| `osgUtil::LineSegmentIntersector` + `cObjectOsgNode` | `TestIntersection` → `SdfPath` → registry | ✅ | thin-geo picking needs `resolveDeep`/proxies (Risk R2) |

## osgEarth / geospatial  *(see `docs/05`)*

| osgEarth | Replacement | Clean? |
|---|---|---|
| `MapNode` (tiled terrain) | static USD earth/terrain (P1); Cesium-native 3D Tiles (P3) | ⚠️/❌ |
| `GeoTransform` | `UsdGeoAnchor` (PROJ ECEF + ENU) | ⚠️ |
| `GeoPoint`, `SpatialReference` (ECEF/transform) | PROJ 9.x (EPSG:4326↔4978) | ✅ |
| `ElevationQuery` | CPU ray cast on terrain mesh (P1); cesium-native (P3) | ⚠️ |
| `Viewpoint` + `EarthManipulator` | `cOsgCanvas::EarthViewpoint` + viewer earth controller | ⚠️ |
| `SkyNode` | static skybox sphere | ⚠️ no time-of-day |
| Annotation `LabelNode` | viewer HUD text | ⚠️ no declutter |
| Annotation `CircleNode` | geo-anchored `UsdGeomBasisCurves` | ⚠️ no terrain drape |
| Annotation `FeatureNode`/`LineString` | `UsdGeomBasisCurves` in ECEF | ✅ |
| Annotation `RectangleNode` | `UsdGeomMesh` quad | ⚠️ no clamp |
| feature symbology / building extrusion | — | ❌ osgEarth-specific (the `boston.earth` 3D buildings) |

## Not used anywhere (no work needed)
`osgFX`, `osgShadow`, `osgParticle` — absent. `osgAnimation` — included once, never
instantiated (the "animation" is manual material-alpha tweaking).

---

### Legend
✅ faithful 1:1 mapping · ⚠️ workable with a workaround / documented fidelity gap ·
❌ no native USD equivalent — handled viewer-side or deferred.
