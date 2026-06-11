# 02 — Target Architecture

How OMNeT++ + INET render 3D after OSG and osgEarth are gone. The design exploits the
existing renderer seam, swaps the substrate to OpenUSD/Hydra, and pushes the OSG-shaped
concepts that USD lacks (camera manipulators, text, billboards, lighting) into the viewer
where they belong.

---

## 1. The big picture

```
                 model code (INET visualizer/usd, user models)
                            │  authors USD via thin helper (UsdSceneBuilder)
                            ▼
        ┌──────────────────────────────────────────────┐
        │  cOsgCanvas  (kept by name; OSG-free)          │
        │   • scene root: renderer-neutral handle        │
        │     (cScene3DNode = osg::Node under WITH_OSG    │
        │      transitionally; USD stage handle at end)   │
        │   • hint structs/enums (already OSG-independent)│
        └──────────────────────────────────────────────┘
                            │ cEnvir::ref/unrefSceneNode
                            ▼
        ┌──────────────────────────────────────────────┐
        │  IOsgViewer / IOsgViewerFactory  (unchanged)   │  ← the seam
        └──────────────────────────────────────────────┘
              │ loadExtensionLibrary("oppqtenv-usd")
              ▼
    ┌────────────────────────────────────────────────────────┐
    │  oppqtenv-usd  (new runtime-loaded plugin)              │
    │   UsdViewer : IOsgViewer  (QOpenGLWidget)               │
    │    • UsdImagingGLEngine  (Hydra 2)                      │
    │       Storm/HgiGL on Linux+Windows, HgiMetal on macOS   │
    │    • own 30 fps heartbeat + SetLightingState            │
    │    • C++ camera controllers (trackball/terrain/overview)│
    │    • TestIntersection → SdfPath → cObject registry      │
    │    • Qt/HUD text overlay (replaces osgText)             │
    │    • StageCache: neutral scene → UsdStage               │
    └────────────────────────────────────────────────────────┘
```

`DummyViewer` (the renamed `DummyOsgViewer`) remains the fallback when no usable USD/GPU
backend exists.

---

## 2. Decision D1 — scene-graph abstraction

We evaluated three approaches:

1. **OSG-compatible facade / shim** — re-export `osg::Group`, `osg::Geode`,
   `osg::PositionAttitudeTransform`, `osg::AutoTransform`, `osgText::Text`, `osg::ref_ptr`,
   etc., backed by USD, so INET/sample code compiles unchanged.
2. **Native USD scene API** — generalize the scene-root type; model code authors USD.
3. **Hybrid** — keep `cOsgCanvas` and its hints, generalize only the opaque scene-root type;
   provide a helper for the common node kinds.

**Decision: (3) as the public-API shape, with (2) as the authoring model.** A *permanent*
shim (1) is rejected as the product architecture because OSG's API is deeply stateful /
retained-mode and carries semantics that do not map onto USD's declarative stage —
`osg::ref_ptr` intrusive ref-counting, `NodeVisitor` traversal (INET's `FindNodesVisitor`),
`AutoTransform` billboards that need the *camera at author time*, and
`dirtyBound/dirtyDisplayList` invalidation. A faithful shim re-creates the very design we are
retiring and would bit-rot the same way.

> **Note (from review):** a *subset* facade (~40 osg types, ~3–6k LOC) over the same
> `UsdSceneBuilder` is a reasonable **optional migration vehicle** for third-party C++ models
> and is the natural way to give early milestones OSG-free validation content. It is **not**
> part of the shipped product and is never linked into it. Whether to build it is decision Q1.
> The earlier "render-time OSG→USD bridge" idea is rejected outright: it would force the USD
> plugin to *link OSG*, which contradicts the goal of removing OSG.

### Scene-root type & the compat-alias direction (review fix)
The kernel never dereferences the scene root, so it can be a renderer-neutral handle. The
**alias must go `cScene3DNode → osg::Node` while `WITH_OSG` is set**, so existing consumers
keep compiling during the transition:

```cpp
// include/omnetpp/scene3dnode.h  (new)
#if defined(WITH_OSG)
namespace osg { class Node; }
namespace omnetpp { using cScene3DNode = osg::Node; }   // existing osg::Node* code compiles
#else
namespace omnetpp { struct cScene3DNode; }              // opaque; USD stage handle behind it
#endif
```

(The earlier draft had this backwards — aliasing `osg::Node` to a new type — which would not
have compiled INET's `OsgScene.cc`, the samples' `OsgScene.cc`, or qtenv's `osg::Node`
downcasts in `osgviewer.cc`. Verified against those files.)

### What changes in the kernel
| File | Change |
|---|---|
| `include/omnetpp/scene3dnode.h` *(new)* | the alias above. |
| `include/omnetpp/cosgcanvas.h` | `osg::Node *scene` → `cScene3DNode *scene`; `setScene/getScene` retyped (identical under `WITH_OSG` thanks to the alias). |
| `include/omnetpp/cenvir.h` | add `refSceneNode/unrefSceneNode(cScene3DNode*)` (non-pure; default-forward to the deprecated osg-named ones). |
| `include/omnetpp/cnullenvir.h`, `src/envir/envirbase.*` | matching no-op overrides. |
| `src/sim/cosgcanvas.cc` | route ref/unref through the generalized methods. |
| `include/omnetpp/osgutil.h` | becomes a thin redirect to the new `usdutil.h` under `WITH_USD`; stays as-is under `WITH_OSG`. |

Binary compatibility is not attempted (a major-version change); **source** compatibility is
preserved for models using only `cOsgCanvas` hints, for `osg::Node*` consumers during the
`WITH_OSG` transition, and for INET via the parallel-tree strategy (D3). At final removal
(M12), `osg::Node*` consumers must be on the USD path.

---

## 3. Coordinate system, up-axis & units (review fix — was missing entirely)

This is a migration footgun and must be a **named decision**, not left implicit:

- **OSG and INET are Z-up.** USD's default is **Y-up**. glTF (our model-conversion
  intermediate) is **mandated Y-up**.
- **USD's default `metersPerUnit` is 0.01** (centimeters); OMNeT++/INET work in meters.

**Decision (Q8):** author every stage with `UsdGeomSetStageUpAxis(stage, UsdGeomTokens->z)`
and `UsdGeomSetStageMetersPerUnit(stage, 1.0)`. The asset pipeline (M7) must bake a
Y-up→Z-up correction into converted `.osgb`→glTF→USD models (a +90° X rotation, or set the
model's `upAxis` on import) so they sit correctly in a Z-up meters scene. The osgDB
pseudo-loader rotation arguments in existing `.ini` files are tuned to OSG's axes — the loader
that re-applies them (M7) must therefore interpret them in the Z-up frame to preserve model
orientation. Without this, every model is rotated 90° and scaled 100×.

---

## 4. Decision D2 — the `oppqtenv-usd` viewer plugin

A new directory `src/qtenv/usd/` builds `liboppqtenv-usd`, mirroring `src/qtenv/osg/`. It
implements the **existing** `IOsgViewer`/`IOsgViewerFactory` (no interface rename for the
first cut). Responsibilities:

| OSG responsibility | USD/Hydra replacement |
|---|---|
| `osgViewer::CompositeViewer` (global) | one `UsdImagingGLEngine` per `UsdViewer`, sharing a global `Hgi`/`HdDriver` |
| `HeartBeat` 30 fps `QBasicTimer` (lives **inside** the OSG plugin) | a **new** equivalent in the USD plugin (cannot reuse the OSG one); gate on `!engine->IsConverged()` |
| `GraphicsWindowEmbedded` bridging OSG↔Qt GL | **eliminated** — `SetPresentationOutput("OpenGL"/metal, defaultFramebufferObject())` composites into the widget |
| implicit OSG headlight | **explicit** `engine->SetLightingState(...)` / a default headlight (Storm has none) |
| `view->setSceneData(node)` | `StageCache` builds a `UsdStage`; `engine.Render(stage->GetPseudoRoot(), params)` in `paintGL()` |
| `camera->setProjectionMatrixAsPerspective(...)` | `GfCamera`/`CameraUtil` → `engine.SetCameraState(view, proj)` |
| `camera->setClearColor` | `UsdImagingGLRenderParams::clearColor` |
| `view->setCameraManipulator(...)` | **C++ camera controllers** (USD has none) — see §4.1 |
| `osgUtil::LineSegmentIntersector` + `cObjectOsgNode` walk | `engine.TestIntersection(...)` → `hitPrimPath` → registry (D4) |
| `osgGA::EventQueue` | direct handling in `mouse*Event`/`wheelEvent`/`key*Event` |
| `osgText::Text` | **Qt/HUD overlay** (USD has no text) — see §4.2 |

`UsdImagingGLEngine` is the canonical embeddable entry point (the API `usdview` uses):
`SetPresentationOutput`, `SetRenderBufferSize`, `SetFraming`, `SetCameraState`,
`SetLightingState`, `Render`, `TestIntersection`, `IsConverged`. With OpenUSD 25.11 making
Hydra 2 the default, the scene-index path is transparent to this surface.

### Per-platform Hgi backend (Decision D6 — review fix)
HgiGL requires **OpenGL ≥ 4.5**. **macOS caps OpenGL at 4.1**, so a "fall back to Dummy if
< 4.5" rule would disable 3D on every Mac. Instead:
- **Linux / Windows:** Storm + **HgiGL** (require GL ≥ 4.5).
- **macOS:** Storm + **HgiMetal** with **hgiInterop** to composite the Metal output into the
  `QOpenGLWidget`'s GL FBO (the path `usdview` uses on macOS). The build must include the
  `hgiMetal` library on macOS.
- Select the backend at `UsdViewer` construction; only fall back to `DummyViewer` if no
  suitable backend is available.

### Shared GL context (review fix)
Sharing one `Hgi`/`HdDriver` across multiple 3D inspectors requires
`QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts)` **before** the `QApplication` is
created. Qtenv does **not** currently set this (verified). M3 must add it in the Qtenv
startup path, or each viewer needs its own `Hgi` (heavier, but isolates contexts).

### `paintGL()` sketch
```cpp
engine->SetPresentationOutput(presentToken /*"OpenGL" or metal*/,
    VtValue((uint32_t)defaultFramebufferObject()));
engine->SetRenderBufferSize(GfVec2i(w*dpr, h*dpr));
engine->SetFraming(framing);
engine->SetCameraState(viewMatrix, projMatrix);
engine->SetLightingState(headlight);                 // Storm needs explicit lighting
UsdImagingGLRenderParams p; p.clearColor = clear; p.frame = UsdTimeCode::Default();
engine->Render(stage->GetPseudoRoot(), p);
// then draw text overlay (see §4.2), GL-state-safe
```

### 4.1 Camera controllers (USD ships none)
Pure math on `GfMatrix4d` (`eye/center/up + distance`, recompute `LookAt` per event):
- **TrackballController** (replaces `TrackballManipulator`), **TerrainController** (elevation
  ≥ 0, XY-plane pan; replaces `TerrainManipulator`), **OverviewController** (port the existing
  fixed-up logic from `cameramanipulators.cc` almost verbatim). CAM_EARTH wired in M10.

### 4.2 Text overlay (USD has no text prim)
Draw labels with `QPainter` over the widget after `Render`. Hydra leaves GL pixel-transfer
state that corrupts `QPainter`; use the verified usdview workaround (render text to a
`QImage`/texture and blit a screen-space quad, or end native rendering before the overlay
pass). Anchors are world positions projected to screen each frame; depth = always-on-top
(matches OSG's depth-off labels).

---

## 5. Decision D4 — picking & object tagging (replacing `cObjectOsgNode`)

USD picking returns an `SdfPath` with no object association. Replace `cObjectOsgNode :
osg::Group` with a **prim-path → object registry**:
- `cPrimObjectBinding` records `(SdfPath, componentId | cObject*)`, mirroring `cObjectOsgNode`'s
  dual storage; the registry lives in `UsdViewer`/`StageCache`.
- The scene builder writes custom prim metadata (`omnetpp:objectId`) and registers the binding.
- `objectsAt(QPoint)` calls `TestIntersection(resolveDeep, …)`, then walks ancestor `SdfPath`s
  (`GetParentPath()`) through the registry — the analogue of the old `NodePath` scan.
- Thin geometry (lines, arrowheads) picks poorly with an ID pass → `resolveDeep` plus, where
  needed, invisible thick pick-proxy meshes alongside the visible curves (Risk R2).
- `usdutil.h` (under `WITH_USD`) provides `cPrimObjectBinding::setObject(const cObject*)`
  matching `cObjectOsgNode::setObject`, so porting call sites is mechanical.

---

## 6. Decision D5 — build system

Introduce `WITH_USD` (and `WITH_USD_GEO`) **alongside** `WITH_OSG`/`WITH_OSGEARTH`
transitionally; **remove the OSG/osgEarth ones at parity** (M12).

- `configure.in`: add an OpenUSD detection block — probe headers (`pxr/usd/usd/stage.h`,
  `pxr/usdImaging/usdImagingGL/engine.h`) and the monolithic `usd_ms` lib; set
  `USD_CFLAGS`/`USD_LIBS`; `AC_DEFINE(WITH_USD)`. Probe PROJ for `WITH_USD_GEO`.
- `config.h.in`: `#undef WITH_USD`, `#undef WITH_USD_GEO`.
- `Makefile.inc.in`: `WITH_USD`, `USD_CFLAGS`, `USD_LIBS`; extend `QTENV_LIBS`.
- `src/qtenv/Makefile`: build `usd/` subdir when `WITH_USD=yes`.
- `src/qtenv/usd/Makefile`: produce `oppqtenv-usd`; `IMPLIBS += $(USD_LIBS)`; **on macOS link
  `hgiMetal`**; set `QT_NO_KEYWORDS` (**required** — Qt moc keywords collide with USD/TBB/
  boost-python headers).
- `iosgviewer.cc`: prefer `loadExtensionLibrary("oppqtenv-usd")` when `WITH_USD`, else the OSG
  lib during transition — ideally a single plugin-name string from `Makefile.inc`.
- `DummyOsgViewer` stays as the fallback; message generalizes to "3D Viewer library".

**OpenUSD packaging:** minimal imaging config — `PXR_BUILD_IMAGING=ON`,
`PXR_BUILD_USD_IMAGING=ON`, monolithic `usd_ms`, **HgiMetal on macOS / HgiGL elsewhere**,
`PXR_BUILD_TESTS/EXAMPLES/TUTORIALS/USDVIEW=OFF`, Alembic/OpenVDB/OCIO off unless needed
(~30 MB vs OSG's ~20 MB across libs). Distribute as an **optional dependency**, as OSG was;
prefer prebuilt (vcpkg/conan/Homebrew) where available. Plugin discovery via
`PXR_PLUGINPATH_NAME` or compiled-in metadata. **Windows toolchain is an open decision (Q9):**
OpenUSD is effectively MSVC-built while OMNeT++ ships MinGW — these are ABI-incompatible.

**Inspector descriptors (review fix):** `osg.msg`/`osg_m.*` register the `cOsgCanvas` enums
and the scene "Fields"/tree browsing **inside the OSG plugin**; a USD-only build loses them.
M2 adds a `usd.msg` (or moves these descriptors into the kernel/qtenv proper) so the inspector
keeps working without OSG.

---

## 7. INET integration (summary; detail in `docs/03` and `docs/05`)

Per D3, INET gets a **parallel `src/inet/visualizer/usd/` tree** subclassing the same `base/`
classes, sharing a `UsdUtils`/`UsdScene` chokepoint mirroring `OsgUtils`/`OsgScene`. New
opp-feature `VisualizationUsd`. **Deprecated NED alias modules** keep showcases/`.ini` files
that hard-code typenames (e.g. `SceneOsgEarthVisualizer`, `IntegratedOsgVisualizer`) working
through the rename. The geospatial pieces (`SceneOsgEarthVisualizer`, `OsgEarthGround`,
`OsgGeographicCoordinateSystem`) are **re-implemented** on USD/PROJ behind `WITH_USD_GEO`;
osgEarth is removed, not retained (D4).
