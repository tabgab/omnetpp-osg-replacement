# 02 — Target Architecture

How OMNeT++ + INET render 3D after OSG is gone. The design exploits the existing renderer
seam, swaps the substrate to OpenUSD/Hydra, and pushes the OSG-shaped concepts that USD
lacks (camera manipulators, text, billboards) into the viewer where they belong.

---

## 1. The big picture

```
                 model code (INET visualizer/usd, user models)
                            │  authors USD via thin helper
                            ▼
        ┌──────────────────────────────────────────────┐
        │  cOsgCanvas  (kept by name; OSG-free)          │
        │   • scene root: renderer-neutral handle        │
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
    │    • UsdImagingGLEngine  (Hydra 2 / Storm GL)           │
    │    • C++ camera controllers (trackball/terrain/overview)│
    │    • TestIntersection → SdfPath → cObject registry      │
    │    • Qt/HUD text overlay (replaces osgText)             │
    │    • StageCache: neutral scene → UsdStage               │
    └────────────────────────────────────────────────────────┘
```

`DummyViewer` (the renamed `DummyOsgViewer`) remains the fallback when USD is unavailable or
the GL context is too old.

---

## 2. Decision D1 — scene-graph abstraction

We evaluated three approaches:

1. **OSG-compatible facade / shim** — re-export `osg::Group`, `osg::Geode`,
   `osg::PositionAttitudeTransform`, `osg::AutoTransform`, `osgText::Text`, `osg::ref_ptr`,
   etc., backed by USD, so INET/sample code compiles unchanged.
2. **Native USD scene API** — replace the scene-root type with a USD stage handle; model
   code authors USD.
3. **Hybrid** — keep `cOsgCanvas` and its hints, swap only the opaque scene-root type for a
   neutral handle; provide a helper for the common node kinds.

**Decision: (3) as the public-API shape, with (2) as the authoring model — and an explicit
rejection of a *permanent* (1).**

Why not the shim: OSG's API is deeply stateful/retained-mode and carries semantics that do
not map onto USD's declarative, compositional stage — `osg::ref_ptr` intrusive ref-counting,
`NodeVisitor` traversal (INET's `FindNodesVisitor`), `AutoTransform` billboards that need the
*camera at author time*, and `dirtyBound/dirtyDisplayList` invalidation. A faithful shim is
6–10k lines of fragile code that **re-creates the very design we are retiring** — it would
bit-rot the same way. Worse, it would keep an OSG build dependency unless every shimmed type
were fully stubbed.

What we do instead:
- `cOsgCanvas` keeps its name, all hint structs, and all enums (they are already
  OSG-independent). Its scene-root field changes from `osg::Node*` to a **forward-declared,
  renderer-neutral handle** — call it `cScene3DNode*` (an opaque pointer the kernel never
  dereferences). The kernel stays renderer-agnostic and link-free of both OSG and USD.
- Model code (ported INET, new user models) authors USD through a **thin helper layer**
  (`UsdSceneBuilder`/`UsdUtils` — see `docs/03-roadmap.md`), not raw `pxr::` everywhere. The
  helper owns the `UsdStage`; the neutral handle the canvas carries points at it.
- A **render-time OSG→USD bridge** is offered *only* as an optional, clearly-temporary
  migration aid (a `liboppscene-osgcompat` that converts an `osg::Node` tree to USD on
  `setScene`), for third-party C++ models that cannot be ported immediately. It is **not**
  part of the end-state and pulls OSG in only when explicitly enabled.

### What changes in the kernel
| File | Change |
|---|---|
| `include/omnetpp/cosgcanvas.h` | `osg::Node *scene` → `cScene3DNode *scene`; `setScene/getScene` retyped; new `include/omnetpp/scene3dnode.h` forward-declares the handle. Keep `using OsgNode = cScene3DNode;` under a compat macro for one cycle. |
| `include/omnetpp/cenvir.h` | add `refSceneNode/unrefSceneNode(cScene3DNode*)` (non-pure; default-forward to the old osg-named ones); deprecate `refOsgNode/unrefOsgNode`. |
| `include/omnetpp/cnullenvir.h`, `src/envir/envirbase.*` | add matching no-op overrides. |
| `src/sim/cosgcanvas.cc` | route ref/unref through the generalized methods. |
| `include/omnetpp/osgutil.h` | becomes a thin redirect to the new `usdutil.h` under `WITH_USD`; stays as-is under `WITH_OSG`. |

Binary compatibility is not attempted (this is a major-version change); **source**
compatibility is preserved for models that only use `cOsgCanvas` hints, and for INET via the
parallel-tree strategy (D3).

---

## 3. Decision D2 — the `oppqtenv-usd` viewer plugin

A new directory `src/qtenv/usd/` builds `liboppqtenv-usd`, mirroring `src/qtenv/osg/`. It
implements the **existing** `IOsgViewer`/`IOsgViewerFactory` (no interface rename needed for
the first cut; an optional later rename to `IScene3DViewer` is cosmetic). Responsibilities map
one-to-one:

| OSG responsibility | USD/Hydra replacement |
|---|---|
| `osgViewer::CompositeViewer` (global) | one `UsdImagingGLEngine` per `UsdViewer`, sharing a global `HdDriver`/`Hgi` |
| `HeartBeat` 30 fps `QBasicTimer`, render-when-needed | **same class**; gate on `!engine->IsConverged()` instead of `checkNeedToDoFrame()` |
| `GraphicsWindowEmbedded` bridging OSG↔Qt GL | **eliminated** — `UsdImagingGLEngine::SetPresentationOutput("OpenGL", defaultFramebufferObject())` composites straight into the widget's FBO |
| `view->setSceneData(node)` | `StageCache` builds a `UsdStage`; `engine.Render(stage->GetPseudoRoot(), params)` in `paintGL()` |
| `camera->setProjectionMatrixAsPerspective(fovy,aspect,zn,zf)` | `GfCamera`/`CameraUtil` → `engine.SetCameraState(view, proj)` |
| `camera->setClearColor` | `UsdImagingGLRenderParams::clearColor` |
| `view->setCameraManipulator(...)` | **C++ camera controllers** (USD has none) — see §3.1 |
| `osgUtil::LineSegmentIntersector` + `cObjectOsgNode` walk | `engine.TestIntersection(...)` → `hitPrimPath` → registry (see D4) |
| `osgGA::EventQueue` | direct handling in `mouse*Event`/`wheelEvent`/`key*Event` |
| `osgText::Text` | **Qt/HUD overlay** (USD has no text) — see §3.2 |

`UsdImagingGLEngine` is the canonical embeddable entry point (the same API `usdview` uses):
`SetPresentationOutput`, `SetRenderBufferSize`, `SetFraming`, `SetCameraState`, `Render`,
`TestIntersection`, `IsConverged`. With OpenUSD 25.11 making **Hydra 2** the default, the
scene-index path is transparent to this public surface — the plugin does not touch scene-index
internals. (Verify exact signatures against the OpenUSD version pinned at build time; see
`docs/06` Risk R3 for the GL-state-interop spike that must precede M3.)

`paintGL()` sketch:
```cpp
engine->SetPresentationOutput(TfToken("OpenGL"),
    VtValue((uint32_t)defaultFramebufferObject()));
engine->SetRenderBufferSize(GfVec2i(w*dpr, h*dpr));
engine->SetFraming(framing);
engine->SetCameraState(viewMatrix, projMatrix);
UsdImagingGLRenderParams p; p.clearColor = clear; p.frame = UsdTimeCode::Default();
engine->Render(stage->GetPseudoRoot(), p);
// then draw text overlay (see §3.2), GL-state-safe
```

### 3.1 Camera controllers (USD ships none)
Implement as pure math on `GfMatrix4d`, storing `eye/center/up + distance`, recomputing
`LookAt` per event:
- **TrackballController** — orbit/zoom/pan (replaces `TrackballManipulator`).
- **TerrainController** — trackball constrained to elevation ≥ 0, pan on the XY plane
  (replaces `TerrainManipulator`).
- **OverviewController** — port the existing fixed-up logic from `cameramanipulators.cc`
  almost verbatim (`osg::Vec3d`→`GfVec3d`, stored `_distance`).
- **CAM_EARTH** — earth-style orbit, wired in the geospatial phase (`docs/05`).

### 3.2 Text overlay (USD has no text prim)
After `engine->Render(...)`, draw labels with `QPainter` over the widget — but Hydra leaves
GL pixel-transfer state that corrupts `QPainter`. The verified workaround (the usdview HUD
approach): render text to a `QImage`/texture and blit a screen-space quad, or end native GL
rendering before compositing the overlay. Text anchors are world positions projected to
screen each frame. Depth: OSG drew labels with depth-test off/override anyway, so
"always on top" is the correct, easy behavior.

---

## 4. Decision D4 — picking & object tagging (replacing `cObjectOsgNode`)

USD picking returns an `SdfPath` (prim path) with no object association. Replace
`cObjectOsgNode : osg::Group` with a **prim-path → object registry**:
- A `cPrimObjectBinding` records `(SdfPath, componentId | cObject*)`, mirroring
  `cObjectOsgNode`'s dual storage. The registry lives in the `UsdViewer`/`StageCache`.
- When the scene builder authors a prim that represents a tagged object, it also writes
  custom prim metadata (e.g. `omnetpp:objectId`) **and** registers the binding.
- `objectsAt(QPoint)`:
  ```cpp
  engine->TestIntersection(pickParams /*resolveDeep*/, view, proj,
                           stage->GetPseudoRoot(), params, &results);
  for (auto& r : results) {            // walk ancestors, like the old NodePath scan
      for (SdfPath p = r.hitPrimPath; !p.IsAbsoluteRootPath(); p = p.GetParentPath())
          if (cObject* o = registry.lookup(p)) { out.push_back(o); break; }
  }
  ```
- `usdutil.h` (under `WITH_USD`) provides `cPrimObjectBinding` with a `setObject(const
  cObject*)` API matching `cObjectOsgNode::setObject`, so porting call sites is mechanical.

Thin geometry (lines, arrowheads) picks poorly with an ID pass — mitigate with `resolveDeep`
and, where needed, invisible thick pick-proxy meshes alongside the visible curves
(see `docs/06` Risk R2).

---

## 5. Decision D5 — build system

Introduce `WITH_USD` (and `WITH_USD_GEO`) **alongside** `WITH_OSG`/`WITH_OSGEARTH`; both
coexist during transition.

- `configure.in`: add an OpenUSD detection block after the OSG block — probe headers
  (`pxr/usd/usd/stage.h`, `pxr/usdImaging/usdImagingGL/engine.h`) and the monolithic
  `usd_ms` lib; set `USD_CFLAGS`/`USD_LIBS`; `AC_DEFINE(WITH_USD)`.
- `config.h.in`: `#undef WITH_USD`, `#undef WITH_USD_GEO`.
- `Makefile.inc.in`: `WITH_USD`, `USD_CFLAGS`, `USD_LIBS`; extend `QTENV_LIBS` when enabled.
- `src/qtenv/Makefile`: build `usd/` subdir when `WITH_USD=yes`.
- `src/qtenv/usd/Makefile`: produce `oppqtenv-usd`; `IMPLIBS += $(USD_LIBS)`; set
  `QT_NO_KEYWORDS` (**required** — Qt moc keywords collide with USD/TBB/boost-python headers).
- `iosgviewer.cc`: prefer `loadExtensionLibrary("oppqtenv-usd")` when `WITH_USD`, else
  `oppqtenv-osg` — ideally driven by a single plugin-name string from `Makefile.inc`.
- `DummyOsgViewer` stays as the fallback; its message generalizes to "3D Viewer library".

**OpenUSD packaging:** build a minimal imaging config — `PXR_BUILD_IMAGING=ON`,
`PXR_BUILD_USD_IMAGING=ON`, monolithic `usd_ms`, and `PXR_BUILD_TESTS/EXAMPLES/TUTORIALS/
USDVIEW=OFF`, Alembic/OpenVDB/OCIO off unless needed (~30 MB vs OSG's ~20 MB across libs).
Distribute as an **optional dependency**, exactly as OSG was. Plugin discovery via
`PXR_PLUGINPATH_NAME` or compiled-in plugin metadata.

**Deprecation path:** once USD reaches parity (M11), `configure.in` emits a deprecation
warning for `WITH_OSG`/`WITH_OSGEARTH`; they are removed in a later major release, after
which no `osg::`/`osgEarth::` symbol remains on the supported path.

---

## 6. INET integration (summary; full detail in `docs/03` and `docs/05`)

Per Decision D3, INET gets a **parallel `src/inet/visualizer/usd/` tree** subclassing the
same `base/` abstract classes, sharing a `UsdUtils`/`UsdScene` chokepoint that mirrors
`OsgUtils`/`OsgScene`. New opp-feature `VisualizationUsd` (`-DINET_WITH_VISUALIZATIONUSD`,
`nedPackages=inet.visualizer.usd`). OSG stays selectable. The geospatial pieces
(`SceneOsgEarthVisualizer`, `OsgEarthGround`, `OsgGeographicCoordinateSystem`) get
USD/PROJ-based replacements behind `WITH_USD_GEO`.
