# Development Plan — Replacing OpenSceneGraph with OpenUSD in OMNeT++ & INET

**Status:** Planning complete (revised after an adversarial review — see [`docs/07-plan-review.md`](docs/07-plan-review.md)), implementation not started
**Target trees:** OMNeT++ 6.4.0aipre2 (`~/DEV/omnetpp-6.4.0aipre2`), INET 4.6.0 (`samples/inet-4.6.0`)
**Renderer target:** OpenUSD ≥ 25.11 with Hydra 2; **Storm/HgiGL** on Linux & Windows, **Storm/HgiMetal** on macOS; embedded via `UsdImagingGLEngine`

---

## 1. Context

OMNeT++ and INET use OpenSceneGraph (OSG) + osgEarth for all 3D network visualization:
the Qtenv 3D viewer, network-node models, mobility trails, radio-signal propagation
effects, physical-environment obstacles, and the geospatial/satellite scenarios.

**The problem:** OSG is unmaintained and will bit-rot — future compilers, OSes, Qt
versions, and GPU drivers will break it with no upstream to fix it. osgEarth is a heavy,
awkward, niche dependency. Keeping them jeopardizes the long-term viability of OMNeT++'s
3D visualization.

**The intent:** Remove OSG **and** osgEarth **entirely** and replace them with **OpenUSD** —
Apache-2.0, AOUSD-governed, actively developed, with a production GL/Metal renderer (Hydra
Storm) and a clean embeddable C++ API. Preserve the *functionality* for end users (same INET
visualizers, same NED parameters, same showcases, same look-and-feel). Where a sample or an
osgEarth-dependent piece of code cannot be carried over unchanged, **it is re-implemented on
the USD stack** — that is an accepted cost of removing the dead dependencies, not a reason to
retain them.

**The outcome we want:** OMNeT++ and INET build and visualize in 3D with **zero** OSG or
osgEarth dependency, on every supported platform, on a substrate that will still build and
run a decade from now.

This plan was produced by analyzing the OMNeT++ and INET source directly (a `graphify`
knowledge graph of `src/` plus targeted exploration), verifying the current OpenUSD / Hydra
C++ APIs against upstream docs and the AOUSD forum, and then **re-verifying every load-bearing
claim against the real source tree** in an adversarial review (`docs/07-plan-review.md`),
whose findings are folded in here.

---

## 2. Guiding principles

1. **The tree builds and runs at every step.** During development OSG/osgEarth stay
   selectable purely as a *build convenience* so the tree never breaks; this is transitional,
   not a shipped option. The USD path reaches parity, then OSG **and** osgEarth are removed.
2. **Exploit the existing seam.** Qtenv already loads its 3D renderer as a runtime plugin
   (`oppqtenv-osg`) behind the `IOsgViewer`/`IOsgViewerFactory` interface, with a
   `DummyOsgViewer` fallback. The new USD renderer is a sibling plugin (`oppqtenv-usd`).
3. **Kill OSG and osgEarth — don't clone them, don't keep them.** We do **not** build a
   permanent "mini-OSG-over-USD" shim (it would re-create OSG's stateful design and rot the
   same way), and we do **not** keep osgEarth around behind a flag. INET, the bundled samples,
   and the osgEarth-dependent code are ported to USD natively. The end product contains no
   `osg::` or `osgEarth::` symbol on any supported path.
4. **Be honest about fidelity.** A few OSG/osgEarth capabilities have no clean USD equivalent
   (screen-facing billboards, native text, inline GLSL effects, **live tiled terrain &
   feature symbology**). The plan states exactly where behavior is preserved, where it is
   degraded but acceptable, and where higher fidelity is a later enhancement. **Reduced
   visual fidelity in re-implemented geospatial samples is an accepted tradeoff** — simulation
   behavior is never affected.
5. **Seamless = functionality preserved for end users, not source-compat for all old C++.**
   INET NED modules, showcases, and `.ini` configurations keep working (model file extension,
   a few parameter/typename changes via deprecated NED aliases aside). The four `osg-*`
   samples and any osgEarth-dependent code are **re-implemented** on USD. Custom third-party
   C++ models that built `osg::Node` graphs directly will need source migration (a guide is
   provided); making *those* compile unchanged is explicitly **not** a goal.

---

## 3. Key strategy decisions

Rationale and alternatives are in [`docs/02-architecture.md`](docs/02-architecture.md) and
[`docs/06-risks-and-decisions.md`](docs/06-risks-and-decisions.md).

### D1 — Scene-graph abstraction: **renderer-neutral handle + native USD authoring**
`cOsgCanvas` keeps its name and all its hint structs/enums (already OSG-independent). Its
scene-root field generalizes from `osg::Node*` to a **renderer-neutral handle**
(`cScene3DNode`). During the transition, `scene3dnode.h` aliases `cScene3DNode = osg::Node`
**when `WITH_OSG` is set** so existing OSG consumers keep compiling; in the USD end-state the
handle is a USD-stage handle and OSG consumers are ported. New/ported model code authors USD
through a thin helper (`UsdSceneBuilder`/`UsdUtils`), not raw `pxr::` everywhere.
**Rejected:** a permanent OSG-compatible facade/shim as the product architecture. A
**subset facade** *may* be built as an optional, clearly-non-product migration vehicle for
third-party models (see Q1); the bridge/facade never ships linked into the product.

### D2 — Viewer: a new `oppqtenv-usd` plugin implementing the existing `IOsgViewer` interface
Backed by `UsdImagingGLEngine` inside a `QOpenGLWidget`. It implements its own on-demand
30 fps heartbeat (the OSG `HeartBeat` lives inside the OSG plugin and is not reusable),
reuses the inspector hosting and plugin-loading machinery, and sets up **lighting**
(Storm has no implicit headlight). USD has **no** camera manipulators or text — those are
implemented viewer-side (pure C++ camera controllers; Qt/HUD text overlay). Picking uses
`UsdImagingGLEngine::TestIntersection` + a prim-path→`cObject*` registry.

### D3 — INET migration: a **parallel `visualizer/usd/` tree** sharing a `UsdSceneBuilder` chokepoint
Mirror `visualizer/osg/` with USD implementations subclassing the same renderer-agnostic
`base/` classes. New `VisualizationUsd` opp-feature. Most of the ~90 files are thin; the work
concentrates in the shared helper and a handful of infrastructure classes. **Deprecated NED
alias modules** keep existing showcases/`.ini` files (which hard-code typenames like
`SceneOsgEarthVisualizer`) working through the rename.

### D4 — Geospatial: **osgEarth is removed**; PROJ + native USD earth/terrain is the committed baseline
osgEarth has no drop-in USD replacement, so we build one: PROJ-based coordinate conversion +
a native USD earth sphere / terrain mesh + reimplemented GeoTransform/GeoPoint/annotation
equivalents, **shipped as the product** (not optional). The `osg-earth`/`osg-satellites`
samples and INET's `SceneOsgEarthVisualizer`/`OsgEarthGround`/`OsgGeographicCoordinateSystem`
are **re-implemented** on this stack, accepting documented fidelity changes (no terrain drape,
no building extrusion in v1). Live tiled terrain (Cesium-native 3D Tiles → USD) is an
**optional later fidelity enhancement** — it is *not* a prerequisite for removing osgEarth.

### D5 — Build: introduce `WITH_USD` alongside `WITH_OSG` transitionally; **remove OSG & osgEarth at parity**
New `WITH_USD` / `INET_WITH_VISUALIZATIONUSD` (and `WITH_USD_GEO`) macros and opp-features are
added without immediately deleting the OSG ones, so the tree stays buildable. The plugin
loader prefers USD when built. At parity, `WITH_OSG`/`WITH_OSGEARTH`, `src/qtenv/osg/`,
`osgutil.h`, INET `visualizer/osg/`, and the osgEarth-dependent code are **deleted**.

### D6 — Rendering backend per platform: **HgiGL on Linux/Windows, HgiMetal on macOS**
Hydra Storm's HgiGL needs OpenGL ≥ 4.5; macOS caps GL at 4.1, so macOS uses **HgiMetal +
hgiInterop** (the path `usdview` uses on macOS). The viewer selects the Hgi backend at
construction and only falls back to `DummyViewer` if no suitable backend exists. Windows
needs a toolchain decision (USD is effectively MSVC-built; OMNeT++ ships MinGW) — see Q9.

---

## 4. Milestones

Each milestone leaves the tree buildable and runnable. Effort tiers: **[M]** mechanical
(automatable, Sonnet-suitable), **[D]** needs design judgment.

| # | Milestone | Scope | Tier |
|---|---|---|---|
| **M0** | Neutral public API | `cOsgCanvas` scene root → neutral handle (`cScene3DNode = osg::Node` under `WITH_OSG`); `cEnvir` ref/unref generalized; no behavior change | [D] |
| **M1** | `WITH_USD` build plumbing | configure detection, `Makefile.inc`, qtenv sub-build, OpenUSD packaging (incl. HgiMetal on macOS) | [M] |
| **M2** | `oppqtenv-usd` skeleton + `usd.msg` | plugin loads, implements `IOsgViewer`, renders a placeholder; inspector descriptors/`Register_Enum`s ported | [M] |
| **M3** | Hydra renders a stage in Qt | `UsdImagingGLEngine` in `QOpenGLWidget`, per-platform Hgi, lighting, `AA_ShareOpenGLContexts`, camera projection, trackball | [D] |
| **M4** | Scene + picking | build a `UsdStage` from the neutral scene; `TestIntersection` → `cObject*` registry; validate with a **native-USD** test sample | [D] |
| **M5** | Camera controllers + viewer hints | trackball/terrain/overview in C++; apply all `cOsgCanvas` hints | [D]/[M] |
| **M6** | Text overlay | Qt/HUD text replacing `osgText::Text` (GL-state-safe) | [D] |
| **M7** | INET `UsdUtils`/`UsdScene` + node viz | the shared chokepoint; `NetworkNodeUsdVisualization`; asset pipeline incl. **up-axis/units handling** | [D] |
| **M8** | INET visualizer port + NED aliases | annotation, mobility, link/path, environment, medium visualizers; deprecated NED alias modules | mostly [M], medium [D] |
| **M9** | Signal/scene shaders | MaterialX port of wave-fade & floor shaders, with CPU fallback; per-platform lighting | [D] |
| **M10** | Geospatial (osgEarth removal) | PROJ coord system, native earth/terrain, GeoAnchor, annotation equivalents — **shipped** | [D] |
| **M11** | Sample re-implementation + parity | `osg-*` samples → USD equivalents; visual regression vs OSG | [M]/[D] |
| **M12** | Remove OSG & osgEarth | delete the OSG plugin, `osgutil.h`, INET `visualizer/osg/`, osgEarth code & build machinery; migration guide | [M] |
| **M13** *(optional, later)* | Tiled terrain | Cesium-native 3D Tiles → USD streaming (fidelity enhancement) | [D] |

Dependency order: **M0 → M1 → M2 → M3 → M4 → M5 → M6** (OMNeT++ core), then
**M7 → M8 → M9** (INET), then **M10 → M11 → M12** (geospatial, parity, removal). M13 is an
independent later enhancement. The full file-level breakdown is in
[`docs/03-roadmap.md`](docs/03-roadmap.md).

---

## 5. Feature / parity checklist

The replacement is "done" when each works under `WITH_USD=yes`, with OSG/osgEarth **removed**:

**OMNeT++ Qtenv core**
- [ ] 3D inspector opens for any `cOsgCanvas`; on-demand 30 fps render loop
- [ ] Renders on Linux/Windows (HgiGL) **and macOS (HgiMetal)**
- [ ] Default lighting/headlight set up (Storm has none implicitly)
- [ ] Perspective projection from `cOsgCanvas` hints (fovy, zNear/zFar, clear color); correct up-axis (Z) & units
- [ ] Trackball, Terrain, Overview camera manipulators; generic viewpoint applied at startup
- [ ] Mouse/keyboard navigation; left-click picking → correct `cObject`/`cModule` in inspector
- [ ] Inspector "Fields"/scene-tree browsing works (descriptors + `Register_Enum`s ported)
- [ ] Multiple simultaneous 3D inspectors render (shared `Hgi`, `AA_ShareOpenGLContexts`)
- [ ] `DummyViewer` fallback when no usable USD/GPU backend

**INET visualizers** (showcases work via deprecated NED aliases)
- [ ] Network nodes as 3D models (`.usd`) and as 2D icon billboards
- [ ] Node labels and annotation text (info, statistics, radio, energy, queue, gate-schedule)
- [ ] Mobility movement trails
- [ ] Links / routes / paths with arrowheads, dashed/dotted styles
- [ ] Physical-environment obstacles (box, sphere, prism, polyhedron; wireframe)
- [ ] Radio signal propagation: expanding rings and spheres with fade
- [ ] Range circles, LOS lines

**Geospatial (osgEarth removed; v1 fidelity)**
- [ ] Geographic ↔ ECEF ↔ scene coordinate conversion (PROJ)
- [ ] Native USD earth globe (satellite scenarios) with rotation
- [ ] Native USD terrain / map backdrop (ground scenarios)
- [ ] Geo-anchored node placement, range circles, trails (no terrain drape — documented gap)

**Assets & build**
- [ ] `.osgb` stock models converted to `.usd` with correct axis/units; loader handles pseudo-transform URIs
- [ ] `WITH_USD` build on Linux/macOS/Windows; OpenUSD packaged as an optional dependency
- [ ] **No `osg::`/`osgEarth::` symbol remains anywhere** (M12 complete)

---

## 6. How we use AI agents on this project

Per the project working style: **use small/Sonnet-class agents for mechanical and
search-heavy work, reserve large-model judgment for design, reconciliation, and verification.**

- **[M] mechanical milestones/steps** (build plumbing, thin-file ports, include rewrites,
  asset conversion, parity scaffolding, NED aliases) → Sonnet subagents, fanned out per
  file/category.
- **[D] design steps** (the neutral API, per-platform Hydra/Qt interop, the scene→USD builder,
  shaders, geospatial) → large-model design + review, with Sonnet on the surrounding edits.

This plan was built that way: parallel **Sonnet** Explore agents produced the inventory,
parallel **Sonnet** Plan agents drafted the designs, the large model reconciled their
conflicting recommendations, and a **Fable**-model agent adversarially verified the result
against the real source (its findings are in `docs/07-plan-review.md`, folded into these docs).

---

## 7. Immediate next steps

1. Sign off the open decisions in [`docs/06-risks-and-decisions.md`](docs/06-risks-and-decisions.md)
   — notably the **Windows toolchain (Q9)** and **up-axis/units (Q8)**, plus whether to build
   the optional migration facade (Q1) and where INET work lands (Q4).
2. Stand up a minimal OpenUSD build (imaging config: `usdImaging`, `usdImagingGL`, `hdSt`,
   `hgiGL`, **`hgiMetal`**, `hgiInterop`; monolithic `usd_ms`) and a throwaway
   Qt + `UsdImagingGLEngine` spike on **both Linux/GL and macOS/Metal** to de-risk the
   GL/Metal-context interop (Risk R3) **before** committing to M3.
3. Execute **M0 + M1** (neutral API + build plumbing) — non-breaking, unblocks everything.
