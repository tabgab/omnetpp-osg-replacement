# Development Plan — Replacing OpenSceneGraph with OpenUSD in OMNeT++ & INET

**Status:** Planning complete, implementation not started
**Target trees:** OMNeT++ 6.4.0aipre2 (`~/DEV/omnetpp-6.4.0aipre2`), INET 4.6.0 (`samples/inet-4.6.0`)
**Renderer target:** OpenUSD ≥ 25.11 with Hydra 2 / Storm GL render delegate, embedded via `UsdImagingGLEngine`

---

## 1. Context

OMNeT++ and INET use OpenSceneGraph (OSG) + osgEarth for all 3D network visualization:
the Qtenv 3D viewer, network-node models, mobility trails, radio-signal propagation
effects, physical-environment obstacles, and the geospatial/satellite scenarios.

**The problem:** OSG is unmaintained and will bit-rot — future compilers, OSes, Qt
versions, and GPU drivers will break it with no upstream to fix it. osgEarth is a heavy,
awkward, niche dependency. Keeping them jeopardizes the long-term viability of OMNeT++'s
3D visualization.

**The intent:** Replace OSG/osgEarth **entirely** with **OpenUSD** — Apache-2.0,
AOUSD-governed, actively developed, with a production GL renderer (Hydra Storm) and a
clean embeddable C++ API. Preserve the existing functionality **seamlessly**: same INET
visualizer behavior, same NED parameters, same showcases, same look-and-feel. Only the
rendering substrate changes.

**The outcome we want:** OMNeT++ and INET build and visualize in 3D with **zero** OSG or
osgEarth dependency, on a substrate that will still build and run a decade from now.

This plan was produced by analyzing the OMNeT++ and INET source directly (a `graphify`
knowledge graph of `src/` plus targeted exploration) and verifying the current OpenUSD /
Hydra C++ APIs against upstream docs and the AOUSD forum. See the `docs/` directory for the
detailed inventory, architecture, capability mapping, geospatial strategy, and risks.

---

## 2. Guiding principles

1. **The tree builds and runs at every step.** OSG stays selectable and unbroken until the
   USD path reaches parity; only then is OSG deprecated and removed.
2. **Exploit the existing seam.** Qtenv already loads its 3D renderer as a runtime plugin
   (`oppqtenv-osg`) behind the `IOsgViewer`/`IOsgViewerFactory` interface, with a
   `DummyOsgViewer` fallback. The new USD renderer is a sibling plugin (`oppqtenv-usd`).
3. **Kill OSG, don't clone it.** We deliberately do **not** build a permanent
   "mini-OSG-over-USD" shim — that would re-create OSG's stateful, retained-mode design
   (and its eventual rot) inside our own code. INET and samples are ported to author USD
   **natively** through a thin shared helper. (A render-time OSG→USD bridge is offered only
   as an *optional, temporary* migration aid for un-portable third-party models.)
4. **Be honest about fidelity.** A few OSG/osgEarth capabilities have no clean USD
   equivalent (screen-facing billboards, native text, inline GLSL effects, live tiled
   terrain). The plan states exactly where behavior is preserved, where it is degraded but
   acceptable, and where full parity is a later phase.
5. **Minimize churn for end users.** INET NED modules, showcases, and `.ini` configurations
   keep working (model file extension and a few parameter names aside). Source-level
   migration is only required for custom C++ models that built `osg::Node` graphs directly;
   those get a migration guide.

---

## 3. Key strategy decisions

These are the load-bearing decisions; rationale and alternatives are in
[`docs/02-architecture.md`](docs/02-architecture.md) and
[`docs/06-risks-and-decisions.md`](docs/06-risks-and-decisions.md).

### D1 — Scene-graph abstraction: **renderer-neutral handle + native USD authoring**
`cOsgCanvas` keeps its name and all its hint structs (`Viewpoint`, `EarthViewpoint`,
camera-manipulator/viewer-style enums — these are already OSG-independent). Its scene-root
field changes from `osg::Node*` to a **forward-declared, renderer-neutral handle**
(`cScene3DNode*`/USD stage handle). New and ported model code authors USD through a thin
helper layer rather than touching `pxr::` directly everywhere.

**Rejected:** a permanent OSG-compatible facade/shim. It fights OSG's semantics
(`osg::ref_ptr`, `NodeVisitor`, `AutoTransform` needing the camera at author-time,
`dirtyDisplayList`) and would perpetuate the design we are trying to retire.

### D2 — Viewer: a new `oppqtenv-usd` plugin implementing the existing `IOsgViewer` interface
Backed by `UsdImagingGLEngine` inside a `QOpenGLWidget`. Reuses the existing on-demand
30 fps `HeartBeat`, the inspector hosting (`OsgCanvasInspector`), and the plugin-loading
machinery. USD has **no** camera manipulators or text — those are implemented viewer-side
(pure C++ camera controllers; Qt/HUD text overlay). Picking uses
`UsdImagingGLEngine::TestIntersection` + a prim-path→`cObject*` registry, replacing
`osgUtil::LineSegmentIntersector` + `cObjectOsgNode`.

### D3 — INET migration: a **parallel `visualizer/usd/` tree** sharing a `UsdSceneBuilder` chokepoint
Mirror the existing `visualizer/osg/` structure with USD implementations subclassing the
same renderer-agnostic `base/` classes. A new `VisualizationUsd` opp-feature parallels
`VisualizationOsg`. Most of the ~84 files are thin; the real work concentrates in the
shared helper (`UsdUtils`/`UsdScene`) and a handful of infrastructure classes. OSG stays
selectable during the transition.

### D4 — Geospatial: **minimal custom layer (PROJ + static USD earth/terrain) first; tiled streaming later**
osgEarth has no drop-in USD replacement. Phase-1 geospatial = PROJ-based coordinate
conversion + a static textured earth sphere / static terrain mesh + reimplemented
GeoTransform/GeoPoint/annotation equivalents — adequate for INET's satellite/map scenarios
with documented fidelity gaps. Live tiled terrain (via Cesium-native 3D Tiles) is a
deferred, optional Phase 3.

### D5 — Build: introduce `WITH_USD` alongside `WITH_OSG`; deprecate OSG only after parity
New `WITH_USD` / `INET_WITH_VISUALIZATIONUSD` (and `WITH_USD_GEO`) macros and opp-features
are added without removing the OSG ones. The plugin loader prefers USD when built. OSG is
deprecated with a warning once USD reaches parity, then removed in a later major release.

---

## 4. Milestones

Each milestone leaves the tree buildable and runnable. Effort tiers: **[M]** mechanical
(automatable, Sonnet-suitable), **[D]** needs design judgment.

| # | Milestone | Scope | Tier |
|---|---|---|---|
| **M0** | Neutral public API | `cOsgCanvas` scene root → neutral handle; `cEnvir` ref/unref generalized; no behavior change | [D] |
| **M1** | `WITH_USD` build plumbing | configure detection, `Makefile.inc`, qtenv sub-build, OpenUSD packaging | [M] |
| **M2** | `oppqtenv-usd` skeleton | plugin loads, implements `IOsgViewer`, renders a placeholder | [M] |
| **M3** | Hydra renders a stage in Qt | `UsdImagingGLEngine` in `QOpenGLWidget`, camera projection from hints, trackball controller | [D] |
| **M4** | Scene + picking | build a `UsdStage` from the neutral scene; `TestIntersection` → `cObject*` registry | [D] |
| **M5** | Camera controllers + viewer hints | trackball/terrain/overview in C++; apply all `cOsgCanvas` hints | [D]/[M] |
| **M6** | Text overlay | Qt/HUD text replacing `osgText::Text` (GL-state-safe) | [D] |
| **M7** | INET `UsdUtils`/`UsdScene` + node viz | the shared chokepoint; `NetworkNodeUsdVisualization`; asset pipeline | [D] |
| **M8** | INET visualizer port | annotation, mobility, link/path, environment, medium visualizers | mostly [M], medium [D] |
| **M9** | Signal/scene shaders | MaterialX port of wave-fade & floor shaders, with CPU fallback | [D] |
| **M10** | Geospatial Phase 1 | PROJ coord system, static earth/terrain, GeoAnchor, annotation equivalents | [D] |
| **M11** | Sample port + parity validation | `osg-intro/indoor/earth/satellites` → USD; visual regression vs OSG | [M]/[D] |
| **M12** | OSG deprecation | warnings, docs, migration guide; schedule removal | [M] |
| **M13** *(optional, later)* | Tiled terrain | Cesium-native 3D Tiles → USD streaming | [D] |

Dependency order: **M0 → M1 → M2 → M3 → M4 → M5 → M6** (OMNeT++ core, no OSG broken),
then **M7 → M8 → M9** (INET), then **M10 → M11 → M12** (geospatial, parity, deprecation).
M13 is independent and deferrable.

The full file-level breakdown of each milestone is in
[`docs/03-roadmap.md`](docs/03-roadmap.md).

---

## 5. Feature / parity checklist

The replacement is "done" when each of these works under `WITH_USD=yes`, `WITH_OSG=no`:

**OMNeT++ Qtenv core**
- [ ] 3D inspector opens for any `cOsgCanvas`; on-demand 30 fps render loop
- [ ] Perspective projection from `cOsgCanvas` hints (fovy, zNear/zFar, clear color)
- [ ] Trackball, Terrain, Overview camera manipulators; generic viewpoint applied at startup
- [ ] Mouse/keyboard navigation; left-click picking → correct `cObject`/`cModule` in inspector
- [ ] `DummyViewer` fallback when USD unavailable / GL too old

**INET visualizers**
- [ ] Network nodes as 3D models (`.usd`) and as 2D icon billboards
- [ ] Node labels and annotation text (info, statistics, radio, energy, queue, gate-schedule)
- [ ] Mobility movement trails
- [ ] Links / routes / paths with arrowheads, dashed/dotted styles
- [ ] Physical-environment obstacles (box, sphere, prism, polyhedron; wireframe)
- [ ] Radio signal propagation: expanding rings and spheres with fade
- [ ] Range circles, LOS lines

**Geospatial (Phase 1 fidelity)**
- [ ] Geographic ↔ ECEF ↔ scene coordinate conversion (PROJ)
- [ ] Static earth globe (satellite scenarios) with rotation
- [ ] Static terrain / map backdrop (ground scenarios)
- [ ] Geo-anchored node placement, range circles, trails (no terrain drape — documented gap)

**Assets & build**
- [ ] `.osgb` stock models converted to `.usd`; loader handles pseudo-transform URIs
- [ ] `WITH_USD` build on Linux/macOS/Windows; OpenUSD packaged as an optional dependency
- [ ] OSG fully removable: no `osg::`/`osgEarth::` symbol remains on the USD path

---

## 6. How we use AI agents on this project

Per the project working style: **use small/Sonnet-class agents for mechanical and
search-heavy work, reserve large-model judgment for design and reconciliation.**

- **[M] mechanical milestones/steps** (build plumbing, thin-file ports, include rewrites,
  asset conversion runs, parity test scaffolding) → Sonnet subagents, often fanned out in
  parallel per file/category.
- **[D] design steps** (the neutral API, Hydra/Qt GL interop, the scene→USD builder, the
  shader port, geospatial) → large-model design + review, with Sonnet doing the
  surrounding mechanical edits.

This plan itself was built that way: parallel **Sonnet** Explore agents produced the
dependency inventory, parallel **Sonnet** Plan agents drafted the core and INET/geospatial
designs, and the large model reconciled their conflicting recommendations (e.g., shim vs.
parallel-tree) into the decisions in §3.

---

## 7. Immediate next steps

1. Sign off the open decisions in [`docs/06-risks-and-decisions.md`](docs/06-risks-and-decisions.md)
   (notably: native-USD vs. keep an optional OSG bridge; geospatial fidelity target; OpenUSD
   packaging/distribution strategy; whether INET work lands upstream or in a fork).
2. Stand up an OpenUSD build (minimal imaging config: `usdImaging`, `usdImagingGL`, `hdSt`,
   `hgiGL`, `hgiInterop`; monolithic `usd_ms`) and a throwaway Qt+`UsdImagingGLEngine` spike
   to de-risk the GL-context interop (Risk R3) **before** committing to M3.
3. Execute **M0 + M1** (neutral API + build plumbing) — these are non-breaking and unblock
   everything else.
