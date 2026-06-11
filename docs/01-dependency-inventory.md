# 01 — OSG / osgEarth Dependency Inventory

Complete, file-level census of how OMNeT++ 6.4 and INET 4.6 depend on OpenSceneGraph and
osgEarth, plus the model-facing idioms shown by the OSG sample projects. Paths are relative
to the OMNeT++ tree root (`~/DEV/omnetpp-6.4.0aipre2`). Line numbers are from
6.4.0aipre2 and should be treated as approximate anchors.

---

## 1. OMNeT++ core

### 1.1 Public, model-facing API

#### `include/omnetpp/cosgcanvas.h` — `cOsgCanvas : cOwnedObject`
The primary interface between models and the 3D renderer. **Includes no OSG headers** —
`osg::Node` is only forward-declared (`namespace osg { class Node; }`), so the kernel
compiles without OSG.

- Scene root: protected `osg::Node *scene` (≈ L133), set/read via `setScene(osg::Node*)` /
  `getScene()`. **This is the one place the public API leaks an OSG type.**
- Enums (OSG/osgEarth-specific *semantics*, but plain ints): `ViewerStyle {STYLE_GENERIC,
  STYLE_EARTH}`; `CameraManipulatorType {CAM_AUTO, CAM_TERRAIN, CAM_OVERVIEW, CAM_TRACKBALL,
  CAM_EARTH}`.
- Hint structs (**OSG-independent**, OMNeT++-own): `Vec3d`, `Viewpoint` (eye/center/up),
  `EarthViewpoint` (lon/lat/alt/heading/pitch/range).
- Hint methods: `setViewerStyle`, `setClearColor`, `setCameraManipulatorType`,
  `setFieldOfViewAngle`, `setZNear/ZFar/ZLimits/clearZLimits/hasZLimits`,
  `setGenericViewpoint`, `setEarthViewpoint`.
- Ref-counting is done **indirectly** via `getEnvir()->refOsgNode(node)` /
  `unrefOsgNode(node)` (see `src/sim/cosgcanvas.cc` ≈ L42–51) so the kernel never links OSG.

#### `include/omnetpp/osgutil.h` — `cObjectOsgNode : public osg::Group`
The **only public header with a hard OSG compile dependency**. Entirely wrapped in
`#ifdef WITH_OSG`; includes `<osg/Group>`; uses `META_Node(osg, cObjectOsgNode)`. Associates
a `cObject*` (stored as `componentId` or raw pointer) with an OSG subtree so mouse-picking
can map a hit back to a simulation object. Resolved during picking with
`osgUtil::LineSegmentIntersector`.

#### Other public headers
- `include/omnetpp/cmodule.h`: forward-declares `cOsgCanvas`; `cModule::getOsgCanvas()`
  (≈ L916) lazily creates one; `mutable cOsgCanvas *osgCanvas` field (≈ L306).
- `include/omnetpp/cenvir.h`: `namespace osg { class Node; }` (≈ L28); pure virtuals
  `refOsgNode(osg::Node*)` / `unrefOsgNode(osg::Node*)` (≈ L848, L854) — the ref-counting
  bridge.
- `include/omnetpp/cnullenvir.h`: no-op `refOsgNode`/`unrefOsgNode` (≈ L156–157).
- `include/omnetpp/platdep/config.h[.in]`: defines/undefs `WITH_OSG`, `WITH_OSGEARTH`.

### 1.2 Qtenv renderer integration (`src/qtenv/`)

#### The seam — `iosgviewer.h` / `iosgviewer.cc`
Decouples Qtenv from the OSG impl, which lives in a **runtime-loaded** library
`oppqtenv-osg`.
- `IOsgViewer : public QOpenGLWidget` — abstract 3D viewer widget. Statics:
  `createOne()` (calls `loadExtensionLibrary("oppqtenv-osg")`, ≈ iosgviewer.cc:51),
  `uninit()`, `refNode/unrefNode(osg::Node*)`, `isOsgPreferred()`. Virtuals:
  `setOsgCanvas(cOsgCanvas*)`, `getOsgCanvas()`, `enable()/disable()`, `refresh()`,
  `resetViewer()`, `applyViewerHints()`, `objectsAt(QPoint) → vector<cObject*>` (picking);
  signal `objectsPicked(...)`.
- `IOsgViewerFactory` — abstract factory: `createViewer()`, `shutdown()`,
  `refNode/unrefNode`. Global `QTENV_API IOsgViewerFactory *osgViewerFactory` (≈ L28).
- `DummyOsgViewer` — no-op fallback widget (paints an error message) when the OSG lib is
  absent. **Already proves the fallback contract a new backend must honor.**

> **This is the cleanest replacement boundary in the whole stack.** A new backend
> implements `IOsgViewerFactory`/`IOsgViewer`, ships as a runtime-loaded lib, and sets the
> `osgViewerFactory` global on load.

#### The OSG impl — `src/qtenv/osg/`
- `osgviewer.{h,cc}` — `OsgViewer : IOsgViewer`, `RealOsgViewerFactory`. Key mechanics a USD
  viewer must replicate:
  - One global `osgViewer::CompositeViewer`, `SingleThreaded`, `ON_DEMAND` frame scheme;
    `HeartBeat` `QBasicTimer` at 30 fps calls `viewer->frame()` only when needed.
  - `OffscreenGraphicsWindow`/`GraphicsWindow : osgViewer::GraphicsWindowEmbedded` bridge
    OSG's GL context to `QOpenGLWidget`; custom
    `osg::GraphicsContext::WindowingSystemInterface`.
  - Per-view `osgViewer::View` + `osg::Camera`; perspective from hints; `setComputeNearFarMode`.
  - Input: Qt events → `osgGA::EventQueue` (key/mouse/wheel).
  - Picking: `osgUtil::LineSegmentIntersector` + `view->computeIntersections()`, walk
    `osg::NodePath` for `cObjectOsgNode` (≈ osgviewer.cc:554–575).
  - Manipulators: `osgGA::TrackballManipulator`, `osgGA::TerrainManipulator`, custom
    `OverviewManipulator`, and (osgEarth) `EarthManipulator`.
  - `applyViewerHints()` pushes `cOsgCanvas` hints to the camera; `refresh()` sets scene
    data (after `osgUtil::Optimizer::TextureVisitor`).
- `cameramanipulators.{h,cc}` — `OverviewManipulator : osgGA::OrbitManipulator` (custom
  fixed-up orbit, ≈ L72–158). Logic ports directly to a C++ controller over `GfMatrix4d`.
- `osg.msg` → `osg_m.{h,cc}` — msgc-generated descriptors for the Qtenv inspector "Fields"
  tab: registers the `cOsgCanvas` enums/structs and exposes `osg::Node`/`osg::Group`
  children & descriptions for tree browsing. Generated `osg_m.h` includes `<osg/Node>`,
  `<osg/Group>` and bridges OSG types into OMNeT++'s `any_ptr`.
- `osgcanvasinspector.{h,cc}` — `OsgCanvasInspector : Inspector` hosts the viewer via
  `IOsgViewer::createOne()`; registered for `cOsgCanvas*` objects. **No direct OSG dep.**

#### Qtenv hooks — `src/qtenv/qtenv.cc`
- `IOsgViewer::uninit()` at shutdown (≈ L834).
- `Qtenv::refOsgNode`/`unrefOsgNode` delegate to `IOsgViewer::refNode/unrefNode` → factory →
  `node->ref()/unref()` (≈ L3038–3045).

### 1.3 Build system & guards
- **Macros:** `WITH_OSG` (build the `oppqtenv-osg` lib at all) and `WITH_OSGEARTH`
  (osgEarth code paths), set in `include/omnetpp/platdep/config.h` by `configure`.
- **There are essentially no `#ifdef WITH_OSG` guards inside the kernel or qtenv proper** —
  `WITH_OSG` only controls whether the plugin lib gets built. `osgutil.h` is the one
  exception. `#ifdef WITH_OSGEARTH` guards exist only inside the impl lib.
- `configure.in` (≈ L1145–1180): detects OSG ≥ 3.2.0 (`osg/Version.h` + `osgGetVersion()`)
  and osgEarth ≥ 2.7.0; sets `OSG_LIBS` (default `-losg -losgDB -losgGA -losgViewer
  -losgUtil -lOpenThreads`), `OSG_CFLAGS`, `OSGEARTH_LIBS` (`-losgEarth -losgEarthUtil`);
  `AC_DEFINE(WITH_OSG)`/`AC_DEFINE(WITH_OSGEARTH)` (≈ L1432–1437).
- `Makefile.inc.in`: `WITH_OSG`/`WITH_OSGEARTH`/`OSG_*`/`OSGEARTH_*` vars; static-link
  fragments add `$(OSG_LIBS)` to `QTENV_LIBS` and `-losg -lOpenThreads` to `KERNEL_LIBS`.
- `src/qtenv/Makefile`: builds the `osg/` subdir only if `WITH_OSG=yes`.
- `src/qtenv/osg/Makefile`: produces `oppqtenv-osg`; `IMPLIBS += $(OSG_LIBS)`/`$(OSGEARTH_LIBS)`;
  generates `osg_m.*` via `$(MSGC) --msg6 osg.msg`; sets `QT_NO_KEYWORDS`.

### 1.4 The replacement seams (summary)
| Seam | Location | Abstracts |
|---|---|---|
| `IOsgViewer`/`IOsgViewerFactory` | `src/qtenv/iosgviewer.h` | the whole viewer widget + lifecycle (primary plug-in boundary) |
| `loadExtensionLibrary("oppqtenv-osg")` | `iosgviewer.cc:51` | the impl-lib name coupling |
| `cEnvir::refOsgNode/unrefOsgNode` | `cenvir.h:848,854` | scene-root ref counting |
| `cOsgCanvas::scene` (`osg::Node*`) | `cosgcanvas.h:133` | the scene-root type in the model API |
| `cObjectOsgNode` | `osgutil.h` (`WITH_OSG`) | object tagging for picking |
| `osg.msg` descriptors | `src/qtenv/osg/osg.msg` | inspector reflection over OSG nodes |

---

## 2. INET 4.6 (`samples/inet-4.6.0`)

### 2.1 Architecture — a 3-way split under `src/inet/visualizer/`
- `base/` — ~25 **renderer-agnostic** abstract classes (`VisualizerBase`,
  `SceneVisualizerBase`, `NetworkNodeVisualizerBase`, `MobilityVisualizerBase`,
  `MediumVisualizerBase`, `LinkVisualizerBase`, `PathVisualizerBase`, …). Canvas and OSG
  impls each independently subclass these.
- `canvas/` — 2D Qt impl using OMNeT++ `cCanvas`/`cFigure`.
- `osg/` — **~90 files**, the 3D OSG impl. Categories: `base/`, `scene/`, `mobility/`,
  `physicallayer/`, `environment/`, `common/`, `linklayer/`, `networklayer/`,
  `transportlayer/`, `flow/`, `power/`, `integrated/`, `util/`.

Typical inheritance: `FooVisualizerBase` (in `base/`) ← `FooOsgVisualizer` (in `osg/…`),
parallel to `FooCanvasVisualizer` (in `canvas/…`).

> The raw OSG **include surface** is modest — roughly **88 `#include <osg…>` lines across ~33
> files** — so the include churn is small; the substantive work concentrates in the ~7
> infrastructure files (`OsgUtils`, `OsgScene`, `NetworkNodeOsgVisualization`,
> `MediumOsgVisualizer`, `SceneOsgVisualizerBase`, `PhysicalEnvironmentOsgVisualizer`,
> `MobilityOsgVisualizer`).

### 2.2 The chokepoints — `src/inet/visualizer/osg/util/`
- **`OsgUtils.{h,cc}`** — the shared OSG abstraction every visualizer calls. Factory fns:
  `createLineGeometry`, `createAnnulusGeometry`, `createCircleGeometry`, `createQuadGeometry`,
  `createPolygonGeometry`, `createPolyline`, `createText` (`osgText::Text`),
  `createAutoTransform` (billboard: `ROTATE_TO_SCREEN`/`ROTATE_TO_AXIS`, `autoScaleToScreen`),
  `createPositionAttitudeTransform`, `createStateSet` (`Material` ambient/diffuse/alpha,
  `GL_BLEND`, `TRANSPARENT_BIN`/`OPAQUE_BIN`), `createLineStateSet` (`LineWidth`,
  `LineStipple` `0xAAAA` dotted/`0xF0F0` dashed), `createTexture` (`Texture2D` `REPEAT`),
  `createImageFromResource` (`osgDB::readImageFile`). `LineNode : osg::Group` = line geode +
  two arrowhead `AutoTransform`s, `setStart/setEnd` + `dirtyBound/dirtyDisplayList`.
- **`OsgScene.{h,cc}`** — `TopLevelScene : osg::Group` (the root set via
  `cOsgCanvas::setScene`) holds one `SimulationScene : osg::Group` (all viz nodes). In Earth
  mode `SimulationScene` is parented under an `osgEarth::GeoTransform`. `FindNodesVisitor<T>
  : osg::NodeVisitor`. Static `TopLevelScene::getSimulationScene(cModule*)` = the **single
  insertion point** every visualizer uses.

### 2.3 Representative visualizers
- **`scene/NetworkNodeOsgVisualization`** — extends both
  `NetworkNodeVisualizerBase::NetworkNodeVisualization` **and**
  `osg::PositionAttitudeTransform`. Path A: 3D model via `osgDB::readNodeFile(.osgb)` +
  `osg::Material`. Path B: 2D icon billboard via `osgDB::readImageFile` +
  `createTexturedQuadGeometry` + `AutoTransform(ROTATE_TO_SCREEN)`. `addAnnotation(node,
  size, priority)` stacks labels above the node.
- **`scene/NetworkNodeOsgVisualizer`** — holds `map<int, ref_ptr<NetworkNodeOsgVisualization>>`;
  per-frame `setPosition/setAttitude`.
- **`mobility/MobilityOsgVisualizer`** — per-node `osg::Geode` trail; `extendMovementTrail`
  mutates a `Vec3Array` + `DrawArrays::setCount` + `dirtyBound/dirtyDisplayList` each frame.
- **`physicallayer/MediumOsgVisualizer`** — signal rings via `createAnnulusGeometry` updated
  per-frame; spheres via `ShapeDrawable(Sphere)` with `setRadius` per-frame; **inline GLSL**
  `osg::Program/Shader/Uniform` for wave+fade (uniforms `waveLength`, `waveAmplitude`,
  `waveOffset`, `fadingFactor`, …); `osg::ImageStream` for animated signal icons.
- **`environment/PhysicalEnvironmentOsgVisualizer`** — `ShapeDrawable(Box/Sphere)`,
  `createPolygonGeometry` for prism/polyhedron faces, `PolygonMode` wireframe.
- **`base/LinkOsgVisualizerBase` / `PathOsgVisualizerBase`** — `LineNode`/polyline + arrowheads;
  many thin concrete subclasses (`DataLink`, `Ieee80211`, `InterfaceTable`, `LinkBreak`,
  `NetworkRoute`, `TransportRoute`, `RoutingTable`, `PacketFlow`, …).
- **Annotation visualizers** (`Info`, `Statistic`, `Radio`, `EnergyStorage`, `LinkBreak`,
  `PacketDrop`, `Queue`, `GateSchedule`) — `osgText::Text` in a `Geode`, attached via
  `NetworkNodeOsgVisualization::addAnnotation`.

### 2.4 osgEarth use in INET (the hard part)
- **`osg/scene/SceneOsgEarthVisualizer`** (`WITH_OSGEARTH` only): `osgDB::readNodeFile(.earth)`
  → `osgEarth::MapNode::findMapNode` → `GeoTransform` → PAT → `SimulationScene`;
  `setViewerStyle(STYLE_EARTH)`; `setEarthViewpoint`.
- **`src/inet/environment/ground/OsgEarthGround.{h,cc}`**: `osgEarth::ElevationQuery/Map/GeoPoint`
  for terrain-height lookup.
- **`src/inet/common/geometry/common/GeographicCoordinateSystem.{h,cc}`**:
  `OsgGeographicCoordinateSystem` uses `osgEarth` MapNode/GeoTransform/GeoPoint, `MapSRS`
  ECEF transforms, `osg::Matrixd` for lat/lon/alt ↔ ECEF ↔ scene-local.
- Guards: `#if defined(WITH_OSGEARTH) && defined(INET_WITH_VISUALIZATIONOSG)`.

### 2.5 Build / features
- `opp_defines.h` (generated) defines `WITH_OSG` when OMNeT++ was built with OSG.
- `.oppfeatures`: feature `VisualizationOsg` (`compileFlags=-DINET_WITH_VISUALIZATIONOSG`,
  `nedPackages=inet.visualizer.osg`, `requires=PhysicalEnvironment VisualizationCommon`,
  `initiallyEnabled=false`) and `VisualizationOsgShowcases`.
- No `.msg` files in `visualizer/osg/`; `.ned` files are module declarations only.

---

## 3. OSG sample projects (model-facing idioms)
`samples/osg-intro`, `osg-earth`, `osg-indoor`, `osg-satellites`. These show what user models
build and hand to `setScene`, and what a migration guide must cover:
- `.osgb` 3D models (`glider`, `cow`, `dumptruck`, `boxman`, `office`, `satellite`, `dishlow`)
  loaded via `osgDB::readNodeFile`.
- `.earth` osgEarth XML maps (`type=geocentric`, OpenStreetMap/readymap XYZ tile layers,
  filesystem tile cache, `<sky>`), loaded via `osgDB::readNodeFile` → `osgEarth::MapNode`.
- **osgDB pseudo-loader transform URIs** in `.ini`, e.g.
  `cow.osgb.2.scale.-90,0,90.rot.0,0,-15e-1.trans` — chained scale/rot/trans applied at load.
- Image textures (`.png/.gif/.jpg`) via `osgDB::readImageFile`.
- Heavy osgEarth in `osg-earth`/`osg-satellites`: `MapNode`, `GeoTransform`, `GeoPoint`,
  `SpatialReference`, `SkyNode`, annotations (`LabelNode`, `CircleNode`, `FeatureNode`,
  `RectangleNode`, `LocalGeometryNode`), features (`Feature`, `LineString`, `MultiGeometry`),
  symbology (`Style`, `TextSymbol`, `LineSymbol`, `PolygonSymbol`, `AltitudeSymbol`
  drape/clamp), `Util::{EarthManipulator, SkyNode, LinearLineOfSightNode, LineOfSightTether}`.

---

## 4. Distinct symbol census (what a USD replacement must cover)

**osg::** `Node, Group, Geode, Geometry, PositionAttitudeTransform, AutoTransform, Switch,
LOD, Camera, ref_ptr, observer_ptr, ShapeDrawable, Sphere, Box, Cone, Vec3/Vec3d/Vec3Array,
Vec4/Vec4d/Vec4Array, Quat, Matrixd, DrawArrays, PrimitiveSet, StateSet, Material, Texture2D,
Texture, Image, ImageStream, Depth, CullFace, LineWidth, LineStipple, PolygonMode,
PolygonOffset, Program, Shader, Uniform, NodeVisitor, NodePath, BoundingSphere, CopyOp,
META_Node` — plus `createTexturedQuadGeometry`.
**osgText::** `Text, Font`.
**osgDB::** `readNodeFile` (.osgb/.earth/models), `readImageFile`, `DatabasePager`.
**osgViewer::** `CompositeViewer, View, GraphicsWindowEmbedded`.
**osgGA::** `TrackballManipulator, TerrainManipulator, OrbitManipulator, CameraManipulator,
EventQueue, GUIEventAdapter`.
**osgUtil::** `Optimizer, LineSegmentIntersector, IntersectionVisitor, UpdateVisitor`.
**osgEarth::** `MapNode, GeoTransform, GeoPoint, Map, SpatialReference (ECEF/transform),
ElevationQuery, Viewpoint, Capabilities, Units`; **Annotation** `LabelNode, CircleNode,
FeatureNode, RectangleNode, LocalGeometryNode`; **Features** `Feature, LineString`;
**Symbology** `Style, MultiGeometry, TextSymbol, LineSymbol, PolygonSymbol, AltitudeSymbol`;
**Util** `EarthManipulator, SkyNode, LinearLineOfSightNode, LineOfSightTether`.
**Not used anywhere:** `osgFX`, `osgShadow`, `osgParticle`. `osgAnimation` is included once but
not actually instantiated.

Usage weight by codebase:

| Capability bucket | OMNeT++ core | INET | Samples |
|---|---|---|---|
| Scene-graph structure | heavy (viewer) | heavy | heavy |
| Geometry & drawables | light | heavy | moderate |
| Appearance / state | moderate | heavy | moderate |
| Text | – | heavy | light |
| Viewer / camera / interaction | **heavy (sole site)** | – | – |
| osgEarth / geospatial | moderate | moderate | heavy |
| Inline GLSL effects | – | moderate | – |

File formats in play: `.osgb` (models), `.earth` (maps), `.png/.gif/.jpg` (textures), the
osgDB pseudo-loader transform-URI syntax, and `osg::ImageStream` animated textures.
