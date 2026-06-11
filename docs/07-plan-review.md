# 07 — Adversarial Plan Review

**Reviewer:** independent technical review, 2026-06-11.
**Scope:** all documents in this repo, verified against the actual trees
(`~/DEV/omnetpp-6.4.0aipre2`, `samples/inet-4.6.0`) and against current OpenUSD
documentation/releases. Every "confirmed" below was checked against a real file:line or a
cited URL; items marked *suspect* were not fully verifiable.

---

## 0. Summary verdict

**The plan is structurally sound and unusually well-grounded in the real code — the seam
analysis, the API census, and the headline USD API claims all check out. It is not
executable as written.** Two milestones violate the plan's own "buildable at every step"
invariant (M0 retypes a public API that INET and the samples call directly; M4–M6
validations depend on an OSG→USD bridge that is never scheduled and that contradicts D1).
Three platform-level facts are missing that materially change the design: **OSG-Z-up vs
USD-Y-up / glTF-Y-up axis handling is never mentioned anywhere**, the macOS GL-4.1 cap
makes the planned "GL ≥ 4.5 else DummyViewer" gate disable 3D on every Mac (the Metal/
hgiInterop path is needed and unplanned), and OpenUSD does not support the MinGW toolchain
that OMNeT++ ships on Windows. The D1 "bridge as migration aid" idea is internally
inconsistent: a render-time `osg::Node`→USD bridge requires linking OSG, so it cannot serve
the project's own "zero OSG dependency" outcome — the dismissed subset-facade is the only
OSG-free compatibility option and was priced unfairly.

Findings: **2 Critical, 6 High, 8 Medium, 8 Low.**

---

## 1. Factual errors & verifications against the OMNeT++/INET code

### 1.1 Confirmed correct (the inventory is largely trustworthy)

| Claim (docs/01) | Verified at |
|---|---|
| `cOsgCanvas` scene root is `osg::Node *scene`, OSG only forward-declared | `include/omnetpp/cosgcanvas.h:23` (`namespace osg { class Node; }`), `:133` (`osg::Node *scene;`) — exact |
| `osgutil.h` = the one public header with a hard OSG dep, fully inside `#ifdef WITH_OSG`, `cObjectOsgNode : public osg::Group`, `META_Node`, componentId/raw-pointer dual storage | `include/omnetpp/osgutil.h:21,23,56–99` — exact |
| `cEnvir::refOsgNode/unrefOsgNode` pure virtuals; fwd-decl | `include/omnetpp/cenvir.h:28,848,854` — exact |
| `cnullenvir.h` no-ops | `include/omnetpp/cnullenvir.h:156–157` — exact |
| `cmodule.h` lazy canvas | `include/omnetpp/cmodule.h:306,916` — exact |
| Ref-counting routed through `getEnvir()->refOsgNode` so the kernel never links OSG | `src/sim/cosgcanvas.cc:42–52` + the design comment at `:18–28` — exact |
| **"Essentially no `#ifdef WITH_OSG` guards in kernel/qtenv proper"** | **CONFIRMED by grep.** Only hits outside the plugin: `osgutil.h:21`, and a build-info string in `src/envir/envirbase.cc:280–285`. `WITH_OSGEARTH` appears only in `src/qtenv/osg/{osgviewer.cc,osgviewer.h,Makefile}` + the same envirbase string. The seam claim holds. |
| `IOsgViewer`/`IOsgViewerFactory`/`DummyOsgViewer`/global `osgViewerFactory`/`loadExtensionLibrary("oppqtenv-osg")` | `src/qtenv/iosgviewer.cc:28,30,50,75–110`; `iosgviewer.h:54–90` — exact (lib load is `:50`, plan says ≈51) |
| OsgViewer mechanics: one global `CompositeViewer`, `SingleThreaded`, `ON_DEMAND`, `HeartBeat`, `GraphicsWindowEmbedded`, `WindowingSystemInterface` | `src/qtenv/osg/osgviewer.cc:95,148,227,262–294,308–327` — exact |
| Picking walk at ≈554–575 | `osgviewer.cc:554–575` (`objectsAt`, `computeIntersections`, `cObjectOsgNode` NodePath scan) — exact |
| `OverviewManipulator : osgGA::OrbitManipulator` | `src/qtenv/osg/cameramanipulators.h:30` — exact |
| `qtenv.cc` hooks | `IOsgViewer::uninit()` at `qtenv.cc:834`; `refOsgNode/unrefOsgNode` at `:3038–3045` — exact |
| `configure.in` OSG/osgEarth detection, default lib lists, `AC_DEFINE`s | `configure.in:1153–1180, 1432–1437` — exact |
| `Makefile.inc.in` static-link fragments (`QTENV_LIBS += $(OSG_LIBS)`, `KERNEL_LIBS += -losg -lOpenThreads`) | `Makefile.inc.in:245–251` — exact |
| `src/qtenv/Makefile` gates `osg/` on `WITH_OSG` | `:120` — exact |
| `osg/Makefile`: `oppqtenv-osg`, `IMPLIBS += $(OSG_LIBS)/$(OSGEARTH_LIBS)`, `$(MSGC) --msg6 osg.msg`, `QT_NO_KEYWORDS` | `src/qtenv/osg/Makefile:11,29,48,52,159` — exact |
| `OsgCanvasInspector` hosts viewer via `IOsgViewer::createOne()`, registered for `cOsgCanvas*`, no direct OSG dep | `src/qtenv/osgcanvasinspector.cc:32,38,42,53` — exact |
| INET 3-way split; `base/` ≈25 abstract classes | `visualizer/base/` = 77 files ≈ 26 classes — correct |
| `OsgUtils` factory functions incl. `createText`, `createAutoTransform`, `LineNode : Group` w/ `setStart/setEnd` | `visualizer/osg/util/OsgUtils.{h:64,84, cc:57–360}` — exact |
| `TopLevelScene/SimulationScene/FindNodesVisitor`, static `getSimulationScene(cModule*)` insertion point | `visualizer/osg/util/OsgScene.{h:23–57, cc:22–45}` — exact |
| `NetworkNodeOsgVisualization` dual inheritance (`NetworkNodeVisualization` + `osg::PositionAttitudeTransform`), readNodeFile path, `ROTATE_TO_SCREEN` icon path, `addAnnotation` | `visualizer/osg/scene/NetworkNodeOsgVisualization.h:21,49, .cc:58,87,124,170` — exact |
| `MediumOsgVisualizer` inline GLSL (`waveLength/waveAmplitude/waveOffset/fadingFactor` uniforms) and `osg::ImageStream` | `visualizer/osg/physicallayer/MediumOsgVisualizer.cc:14,61–68,222–254` — exact |
| osgEarth chokepoints: `SceneOsgEarthVisualizer` (visualizer/osg/scene/), `OsgEarthGround` (`environment/ground/`), `OsgGeographicCoordinateSystem` (`common/geometry/common/`), guards `WITH_OSGEARTH && INET_WITH_VISUALIZATIONOSG` | all confirmed at the stated paths/guards |
| `.oppfeatures` `VisualizationOsg` (`-DINET_WITH_VISUALIZATIONOSG`, `nedPackages=inet.visualizer.osg`, `requires=PhysicalEnvironment VisualizationCommon`, `initiallyEnabled=false`) + `VisualizationOsgShowcases` | `.oppfeatures:1742–1787` — exact |
| Sample idioms: `.osgb`, `.earth` (`boston.earth`, `simple.earth`), pseudo-loader URIs | `samples/osg-earth/omnetpp.ini:14` (`cow.osgb.2.scale.-90,0,90.rot.0,0,-15e-1.trans`), `samples/osg-indoor/omnetpp.ini:11` — exact |
| `osgAnimation` included once, never instantiated | `samples/osg-earth/RambleNode.h:18` includes `<osgAnimation/BasicAnimationManager>`; no instantiation in any `.cc` — confirmed (but see O-10: `osg-earth/makefrag:3` *links* `-losgAnimation`, so removal touches makefrags, and `.osgb` assets may embed animation data) |

### 1.2 Errors / overstatements (confirmed wrong)

- **E1 (Low) — "~250 include sites" (R8) is overstated ~3×.** Actual: 88 `#include <osg...>`
  lines across 33 files under `src/inet/visualizer/osg/` (grep count), plus a handful in
  `common/geometry/common/`, `environment/ground/`, and the samples. The port is *smaller*
  than the risk register says.
- **E2 (Low) — "~84 files" under `visualizer/osg/` is 90** (31 `.cc` + 31 `.h` + 28 `.ned`).
  Right order of magnitude; fix the number and note that 28 of them are NED files whose
  *names* are user-facing (see H-4).
- **E3 (Low) — "Reuses the existing … `HeartBeat`" / "same class" (docs/02 §D2 table) is
  wrong as stated.** `HeartBeat` is declared inside the OSG plugin
  (`src/qtenv/osg/osgviewer.h:46`), not in Qtenv proper. The USD plugin cannot reuse it; it
  must carry its own copy (or the class must first be hoisted into qtenv). Same for the
  floating-toolbar plumbing.
- **E4 (Low) — `osg.msg` enum registration side-effect is missed.** `osg.msg` doesn't just
  describe `osg::Node` for the Fields tab; it executes
  `Register_Enum(cOsgCanvas::ViewerStyle…)` / `Register_Enum(cOsgCanvas::CameraManipulatorType…)`
  (`src/qtenv/osg/osg.msg:19–20`) and registers `cClassDescriptor`s for
  `cOsgCanvas::Viewpoint` etc. (`osg_m.cc:176–266`). Under a USD-only build these
  registrations vanish — the inventory lists `osg.msg` as a seam but the roadmap never
  replaces it (see O-2). (Also note the existing registration is itself incomplete —
  `CAM_TERRAIN`/`CAM_OVERVIEW` are missing from the enum registration — worth fixing in
  passing.)
- **E5 (Low) — "`configure.in` (≈ L1145–1180)"**: block actually starts at 1153. Cosmetic;
  the doc itself says anchors are approximate.

---

## 2. API errors & USD-fact verification

### 2.1 Confirmed correct (the USD surface is real)

- `UsdImagingGLEngine` **is** the canonical embeddable entry point, and every named call
  exists with the claimed semantics: `SetPresentationOutput(TfToken, VtValue)` (framebuffer
  as `VtValue`-wrapped `uint32_t`), `SetCameraState(GfMatrix4d, GfMatrix4d)`,
  `SetRenderBufferSize(GfVec2i)`, `SetFraming(CameraUtilFraming)`, `Render(UsdPrim,
  UsdImagingGLRenderParams)` (params include `clearColor`), `IsConverged()`, and the
  recommended `TestIntersection(PickParams, …, IntersectionResultVector*)` overload with
  `resolveMode`, including **`resolveDeep`** (deep selection through occluders).
  Source: https://openusd.org/dev/api/class_usd_imaging_g_l_engine.html
- **Hydra 2 / scene index default in 25.11 — confirmed.** `USDIMAGINGGL_ENGINE_ENABLE_SCENE_INDEX`
  on by default in 25.11. Source: https://aousd.org/blog/announcing-openusd-v25-11-key-features-and-improvements/
- **R4's "26.x targeted mesh invalidation" — confirmed.** 26.03 added targeted invalidation
  for transforms/extents/points/normals in Storm. Source: https://aousd.org/blog/openusd-v26-03/
- **HgiGL requires OpenGL ≥ 4.5 — confirmed.**
  Source: https://github.com/PixarAnimationStudios/OpenUSD/issues/2756
- **"No native text" — still true.** Autodesk's USD text proposal (ASWF WG, March 2024,
  https://forum.aousd.org/t/usd-text-proposal-presentation-in-todays-aswf-usd-working-group-call/1317)
  has not shipped in core OpenUSD through 26.x as far as the changelog shows. The plan
  should *cite* the proposal as a watch-item (a future `UsdText` would obsolete the HUD-only
  decision Q6), but the gap claim is accurate today.
- "No billboard/AutoTransform", "no animated texture", camera manipulators absent from the
  C++ API — accurate. (`UsdGeomModelAPI` "cards" draw mode is a bounding-card stand-in, not
  a camera-facing billboard; it is not a solution and the plan is right not to use it.)

### 2.2 Errors / gaps

- **A1 (Critical) — The GL-version mitigation in R3 disables 3D on every Mac.** R3 says
  "Check the GL version at `UsdViewer` construction; fall back to `DummyViewer` if < 4.5."
  macOS caps OpenGL at **4.1**, so HgiGL can never run there; as written the USD viewer
  would *always* dummy out on macOS — the platform this plan is being developed on.
  OpenUSD's own answer (what usdview does on Mac) is **HgiMetal + hgiInterop presenting
  into the GL context**. The plan even lists `hgiInterop` in the build config
  (DEVELOPMENT_PLAN.md §7.2) but never states the macOS Metal path, and the architecture
  hard-codes `SetPresentationOutput(TfToken("OpenGL"), …)` thinking in GL-only terms.
  **Fix:** make the spike (docs/06 §C) explicitly three-platform; on macOS create the
  default platform Hgi (Metal) and present via interop into the `QOpenGLWidget`; gate on
  "platform Hgi creatable", not "GL ≥ 4.5".
- **A2 (Medium) — Picking ignores instancing.** The D4 registry walk
  (`hitPrimPath` → `GetParentPath()` loop) is correct for plain prims, but
  `IntersectionResult` carries `instancerContext` precisely because instanced hits do *not*
  resolve through plain ancestor paths. R4 itself proposes `UsdGeomPointInstancer` "for many
  moving entities" — if that mitigation is ever used, D4's picking breaks. Either commit to
  no instancing for pickable prims, or handle `instancerContext` in `objectsAt()` from day 1.
- **A3 (Low) — `LineWidth` → BasisCurves `widths` is marked ✅ but the semantics differ.**
  `osg::LineWidth` is *pixels* (screen-space); `UsdGeomBasisCurves.widths` is *world units*
  (rendered as ribbons/tubes). Width-less linear curves render as thin GL lines in Storm,
  which is closer to INET's look. This row should be ⚠️ with a chosen policy (width-less
  curves + accept 1px, or world-width with distance compensation).
- **A4 (Low) — `IsConverged()` gating is necessary but not sufficient.** Storm converges in
  one frame; the real re-render trigger is *scene/camera dirtiness*, which Hydra does not
  surface the way `checkNeedToDoFrame()` did. The HeartBeat replacement needs its own dirty
  flag (set in `refresh()`/camera events), with `IsConverged()` only relevant for
  progressive delegates. Minor design note, worth writing down before M3.

---

## 3. Omissions (verified absent from all seven documents)

- **O1 (Critical) — Up-axis and units are never mentioned.** Grep for
  `upAxis/Y-up/Z-up/metersPerUnit` across the repo: zero hits. The facts: OSG/INET scenes
  are Z-up (terrain manipulator, INET ENU geo math); USD stages default to **Y-up**
  (`UsdGeomGetStageUpAxis`, fallback Y) and **metersPerUnit = 0.01**; **glTF mandates
  Y-up**, so the proposed `.osgb → glTF → .usd` chain bakes an axis conversion into every
  converted asset, while the scene builder will presumably author Z-up INET coordinates.
  The existing pseudo-loader rot/scale arguments in shipped `.ini` files
  (`cow.osgb.2.scale.-90,0,90.rot...` — that `-90,0,90` *is* an axis fix-up for OSG) were
  tuned for the OSG orientation and will be wrong after conversion. Without a stated
  convention (recommend: author `upAxis = Z`, `metersPerUnit = 1` on all stages; require the
  asset pipeline to re-orient glTF-derived layers; document the pseudo-loader rot semantics
  post-conversion) every milestone from M4 onward produces sideways scenes. This must be a
  named decision (D6) and an M7 acceptance criterion.
- **O2 (High) — `osg.msg` / inspector reflection has no USD replacement anywhere in the
  roadmap.** M2's file list (`usdviewer.*`, `stagecache.*`) contains no `usd.msg`. Losing it
  means (a) the Qtenv object-inspector "Fields" tree can no longer browse the 3D scene
  (today it shows `osg::Node` children/descriptions via `osg_m.cc` descriptors), and (b) the
  `Register_Enum`/`cClassDescriptor` registrations for `cOsgCanvas` types silently disappear
  in USD-only builds (E4). Add a `usd.msg` (or hand-written descriptors) exposing the
  `UsdPrim` hierarchy (`GetChildren()`, type name, path) to M2/M4.
- **O3 (High) — Lighting is never mentioned.** osgViewer provides a default headlight; INET
  materials are authored with ambient/diffuse against it. Storm renders unlit/black-ish
  without lights. `UsdImagingGLEngine::SetLightingState()` (or an authored camera-following
  light / dome light) must be part of M3 and of the parity checklist. Currently absent from
  DEVELOPMENT_PLAN §5, docs/02, docs/03, docs/04.
- **O4 (High) — Windows toolchain reality.** OpenUSD's support matrix is MSVC-only on
  Windows (VERSIONS.md: VS2022; MinGW unmentioned/unsupported), while OMNeT++ ships and
  builds with a MinGW/clang MSYS2 toolchain (`configure.in:250–301`; the bundled
  `tools/win64` env). You cannot link an MSVC-built `usd_ms` into MinGW-built Qtenv (C++
  ABI). Options — building USD under MinGW-clang (unsupported, patch-heavy), switching the
  Windows 3D plugin to an MSVC out-of-process viewer, or dropping Windows 3D for a release —
  are all unpleasant and none is even identified. The checklist line "`WITH_USD` build on
  Linux/macOS/Windows" is currently wishful. Needs its own risk (R11) and a decision.
- **O5 (Medium) — USD runtime resources & plugin discovery are glossed.** Even monolithic
  `usd_ms` requires the on-disk `lib/usd/*/resources/` tree (`plugInfo.json`, Storm
  `.glslfx`, MaterialX stdlib) at runtime; there is no stock "compiled-in plugin metadata"
  option as docs/02 §D5 implies. The "~30 MB vs OSG ~20 MB" size claim is *suspect*
  (unverified; stripped `usd_ms` + resources + TBB + MaterialX libs is plausibly 2–4× that).
  M1 must specify how the relocatable OMNeT++ installation finds the USD resource tree
  (`PXR_PLUGINPATH_NAME`, rpath-relative layout) for a *runtime-loaded* plugin.
- **O6 (Medium) — Multi-viewer GL context sharing.** Qtenv never sets
  `Qt::AA_ShareOpenGLContexts` (grep: absent from `src/qtenv/`). Each `QOpenGLWidget` gets
  its own context; one shared `HgiGL` (R3's mitigation) submits GL objects that are invalid
  in the other widgets' contexts. The mitigation must add `AA_ShareOpenGLContexts` before
  `QApplication` construction in qtenv — or use one engine *per viewer* and share nothing.
  The spike's success criterion "two viewers share an Hgi" will fail without this.
- **O7 (Medium) — Transitional scene-handle typing.** During coexistence (`WITH_OSG` +
  `WITH_USD`), `cOsgCanvas`'s neutral handle may point at an `osg::Node` (OSG-authored
  model) or a USD scene (ported model); nothing in M0/D1 records *which*. The viewer,
  `cEnvir::refSceneNode`, and `IOsgViewer::refNode(osg::Node*)` (whose signatures the plan
  never retypes — `iosgviewer.h:57–58,89–90`) need a discriminated handle (type tag) or the
  plugin loader must pick the viewer matching the scene type per-canvas, not globally.
- **O8 (Medium) — Dependency inventory for USD's own deps.** TBB appears once (in a
  parenthesis about `QT_NO_KEYWORDS`); OpenSubdiv and MaterialX runtime libs never. These
  are hard deps of a Storm-enabled build (VERSIONS.md: OneTBB 2021.x, MaterialX 1.39.x,
  OpenSubdiv 3.6.x) and belong in R9/M1 and in packaging.
- **O9 (Low) — Manual/documentation debt.** The OMNeT++ Simulation Manual and sample
  READMEs teach `cOsgCanvas`+OSG idioms; M12 plans only a "migration guide". Doc rewrite is
  real scope.
- **O10 (Low) — Embedded animation in assets.** `osg-earth/makefrag:3` links
  `-losgAnimation`; `.osgb` characters (e.g. `boxman`) can embed skeletal animation that the
  glTF converter will either carry (→ needs UsdSkel handling, never mentioned) or drop
  (→ verify visual parity). Add an asset-conversion acceptance check.
- **O11 (Low) — PROJ axis-order pitfall.** `EPSG:4326` in PROJ ≥ 6 is lat/lon ordered;
  forgetting `proj_normalize_for_visualization` is the classic bug for the M10 work. One
  sentence in docs/05 would save a day of debugging.
- **O12 (Low) — Color management.** Storm's linear/sRGB output composited into a non-sRGB
  `QOpenGLWidget` FBO will not match OSG's fixed-function colors exactly; the parity
  validation (M11) should compare with tolerance and pick a policy.

*(Probed and found NOT to be omissions: Python bindings — the kernel has none for
`cOsgCanvas`; headless/Cmdenv — `EnvirBase::refOsgNode` no-ops already exist at
`src/envir/envirbase.h:282–283` and the M0 design preserves them; eventlog — no OSG
involvement; static-link KERNEL_LIBS — captured in docs/01 §1.3, though M1 should add the
matching `WITH_USD` static fragment.)*

---

## 4. Sequencing / buildability bugs

- **S1 (Critical) — M0 as specified breaks every existing OSG consumer, violating the
  invariant it claims to validate.** M0 retypes `setScene(osg::Node*)`/`getScene()` to
  `cScene3DNode*` where `cScene3DNode` is "a forward-declared opaque struct", while claiming
  "existing `WITH_OSG=yes` build + an OSG sample render unchanged". But INET calls
  `osgCanvas->setScene(topLevelScene)` with an `osg::Group*`
  (`visualizer/osg/util/OsgScene.cc:45`, `base/SceneOsgVisualizerBase.cc:31`), the samples do
  too (`samples/osg-intro/OsgScene.cc:42`, `osg-satellites/OsgEarthScene.cc:74`), and both
  qtenv and INET *downcast the getter's result*
  (`src/qtenv/osg/osgviewer.cc:493,684` `MapNode::findMapNode(osgCanvas->getScene())`;
  `SceneOsgVisualizer.cc:42` `check_and_cast<TopLevelScene*>(...->getScene())`). An opaque
  unrelated type compiles none of this. The provided compat alias (`using OsgNode =
  cScene3DNode`) points the wrong way. **Fix:** under `WITH_OSG`, define `cScene3DNode` *as*
  `osg::Node` (`namespace osg { class Node; } using cScene3DNode = osg::Node;`) or keep a
  `setScene(osg::Node*)` overload; under USD-only builds make it the neutral struct. Spell
  this out in M0 — it is the difference between a working and a broken first milestone.
- **S2 (High) — M4/M5/M6 validations depend on a deliverable that is never scheduled and
  that contradicts D1.** M4's exit criterion is "`osg-intro` renders its model under USD",
  M5's is `osg-indoor` — but at M4–M6 those samples still author `osg::Node` scenes (they
  are only ported at M11). The only way they render under USD is the "optional, temporary"
  OSG→USD bridge, which (a) appears in no milestone's work items, (b) forces `oppqtenv-usd`
  (or a helper lib) to link OSG — re-introducing the dependency inside the new plugin, and
  (c) needs nontrivial machinery (NodeVisitor walk, material/state translation) that is
  real engineering, not a parenthesis. **Fix:** either schedule the bridge explicitly as
  M4a with its OSG linkage acknowledged, or re-point M4–M6 validation at a small native-USD
  test model (a `usd-intro` written at M4) and keep the samples for M11.
- **S3 (Medium) — The MaterialX/Storm custom-shader question is validated too late.** R5 is
  the second-hardest technical unknown (whether Storm's MaterialX path can express the
  wave-fade with *animated* inputs at interactive rates), but it gets its first contact
  with reality at M9, after the whole INET port has started. Add a MaterialX mini-spike to
  the §C de-risk spike (one annulus, one animated MaterialX input, Storm).
- **S4 (Medium) — M1 static-link fragment missing.** M1 edits `QTENV_LIBS` but not the
  `Makefile.inc.in:245–251` static-link section; a `WITH_USD` static build (the mode that
  today adds `-losg -lOpenThreads` to `KERNEL_LIBS`) is unaddressed — and statically linked
  USD has its own plugin-registration caveats that should be declared out-of-scope
  explicitly if not supported.

---

## 5. Risk-register gaps (beyond R1–R10)

- **R11 (new, High): Windows/MinGW** — see O4.
- **R12 (new, High): macOS Metal path** — see A1; R3's GL-4.5 framing is the wrong gate.
- **R13 (new, Medium): upstream acceptance.** M0 changes public OMNeT++ kernel headers.
  Q4 asks where the *INET* work lands but nothing asks whether the OMNeT++ core maintainers
  accept the neutral-handle API and a new bundled dependency — the entire plan is dead
  without that sign-off. Should be Q0.
- **R14 (new, Medium): NED/ini compatibility churn** — the showcases hard-code OSG type
  names (`showcases/visualizer/osg/earth/omnetpp.ini:7–8`:
  `*.visualizer.osgVisualizer.typename = "IntegratedOsgVisualizer"`,
  `"SceneOsgEarthVisualizer"`). The parallel `visualizer/usd/` tree renames every NED type,
  so the plan's "ini configurations keep working (… a few parameter names aside)" is not
  true for any config that selects an Osg visualizer by typename — which is *every* 3D
  showcase. Mitigation: ship deprecated NED alias modules (`IntegratedOsgVisualizer extends
  IntegratedUsdVisualizer`) or a documented rename table; budget showcase ini churn.
- **R15 (new, Low): look-and-feel parity of line/point rendering** (widths, stipple,
  anti-aliasing) — individually small, collectively the "it looks wrong" risk for M11's
  visual regression.

Mitigations that won't work as written: R3's GL-version fallback (A1); R3's "share one
HdDriver" without `AA_ShareOpenGLContexts` (O6); R8's effort math built on the inflated
~250-include-site figure (harmless direction, but fix it).

---

## 6. Improvements & the D1 re-litigation

### 6.1 D1: was the OSG-style authoring path dismissed too quickly?

**The plan conflates two different "keep authoring OSG-style" options and rejects them with
one argument.**

*Option B1 — render-time bridge (real OSG objects, translated in the viewer).* This is the
literal "models keep authoring OSG scenes, viewer translates to USD" path, and it is the
*most* seamless: zero INET churn, zero sample churn, zero third-party breakage, all NED/ini
names intact. **But it cannot be the end-state for this project's own goal**: model code
still compiles against `osg/*` headers and links the OSG libs — OSG remains a build
dependency and keeps bit-rotting; only the *rendering* moved. The plan is therefore right
to reject B1 as permanent — but should state this reason (dependency survival), which is
much stronger than the semantics argument it gives. Conversely, because B1 is rejected,
it must stop being load-bearing for M4–M6 validation (S2).

*Option B2 — subset facade (OSG-shaped API re-implemented over USD, no OSG linkage).* This
is what the plan prices at "6–10k lines of fragile code" and rejects. That pricing is
unfair against the plan's own census (docs/01 §4): the actually-used surface is ~40 `osg::`
types, of which a third are trivial value types (`Vec3/Vec4/Quat/Matrixd`), `ref_ptr` is an
afternoon, `NodeVisitor` has exactly one consumer (`FindNodesVisitor`,
`OsgScene.h:23`), `dirtyBound/dirtyDisplayList` can be no-ops, and the genuinely hard
members — `AutoTransform`, `osgText::Text`, `ImageStream`, `Program/Shader` — **must be
solved for the native path anyway** (R1, M6, R6, R5). A facade implemented *as a veneer
over the same `UsdSceneBuilder`* is realistically 3–6k LOC of mostly-stable code, and it
buys: INET compiles nearly unchanged, all 28 NED files keep their names, every showcase ini
keeps working, and third-party C++ models port by recompiling. The real costs are: (a) a
permanent compatibility API that new code will keep using (the ecosystem never becomes
USD-native), (b) pointer-stateful authoring constrains the USD-side design (stable handles,
fine-grained dirty tracking), and (c) `osgDB::readNodeFile`-shaped loading still needs the
asset conversion story regardless.

**Recommendation.** Keep D1's native-USD authoring as the end-state for INET and the
bundled samples — they are first-party code, ported once, and this genuinely kills OSG.
But change two things:
1. **Replace the proposed OSG-linking bridge with the B2 subset facade as the migration
   vehicle** for third-party models (and as the M4–M6 validation path). It serves the same
   transitional purpose without re-introducing OSG into the new plugin, and it is built on
   the same `UsdSceneBuilder` you need anyway — so it is incremental cost, not a fork in
   the road. Whether it is later *removed* or kept as a supported compatibility layer can
   be decided on third-party demand (downgrade Q1 from "rejected" to "permanence TBD").
2. **Preserve NED-level seamlessness explicitly** (deprecated alias modules / typename
   mapping), since that — not C++ source compatibility — is what "seamless" means for the
   vast majority of INET users, who never write OSG C++ but do write
   `*.visualizer.osgVisualizer.typename = "..."` in ini files.

### 6.2 Other concrete improvements

- **Record the VulkanSceneGraph alternative.** docs/05 cites `vsgCs` yet the plan never
  documents why VSG (OSG's designated successor, retained-mode, with osg2vsg migration
  tooling) was not chosen as the substrate. There are good reasons (single-maintainer bus
  factor vs AOUSD governance; USD's interchange value; Hydra's renderer plurality) — write
  them down in docs/06, or the decision will be re-litigated by every reviewer.
- **Billboards via a Hydra scene-index filter, not sim-tick authoring.** R1 accepts stale
  billboard orientation between sim ticks. With Hydra 2 (default since 25.11), the viewer
  can install a filtering scene index that injects camera-facing rotations *per render
  frame* without touching the USD stage — eliminating R1's residual gap and keeping
  authoring clean. Worth a spike at M3/M5.
- **Asset pipeline:** prefer **guc** (Apache-2.0, glTF→USD+MaterialX, purpose-built) or
  Adobe's `usdGltf` *file-format plugin* (Apache-2.0) — the latter lets USD reference
  `.gltf` files directly at runtime, potentially removing the second conversion hop
  entirely. osgVerse is MIT-licensed (fine) but its glTF plugin's *write* support is
  undocumented — verify before depending on it, and note plain `osgconv` to `.obj`/`.fbx`
  + Adobe `usdobj`/`usdfbx` as the fallback for these simple assets.
  (Sources: https://github.com/pablode/guc, https://github.com/adobe/USD-Fileformat-plugins,
  https://github.com/xarray/osgverse)
- **Geospatial:** the PROJ + static-earth Phase 1 is sound and well-scoped; the
  cesium-native 3–6-month estimate is believable (vsgCs is the proof of shape). Two notes:
  Cesium's own USD integration (Cesium for Omniverse) is tied to NVIDIA Fabric, not vanilla
  Hydra — correctly not relied upon; and the `.earth`-file retirement should include a
  shim that *recognizes* `.earth` files and emits a helpful migration error rather than a
  load failure.
- **Add `SetLightingState` + a headlight default to M3** (O3) and **author
  `upAxis=Z, metersPerUnit=1`** as a global convention (O1) — both one-line decisions now,
  expensive archaeology later.

---

## 7. Severity-ranked action list (top 10)

| # | Sev | Action |
|---|---|---|
| 1 | **Critical** | Fix M0: under `WITH_OSG`, `cScene3DNode` must alias `osg::Node` (or keep a typed overload); as written M0 breaks INET (`OsgScene.cc:45`), samples (`osg-intro/OsgScene.cc:42`), and qtenv (`osgviewer.cc:493,684`) and violates the buildable-everywhere invariant. |
| 2 | **Critical** | Rewrite R3's platform gate: macOS = HgiMetal + hgiInterop into the GL widget (GL 4.1 cap makes the "GL ≥ 4.5 else Dummy" rule kill 3D on all Macs); make the spike tri-platform. |
| 3 | **High** | Add the up-axis/units decision (OSG Z-up vs USD Y-up vs glTF Y-up; `metersPerUnit`): author `upAxis=Z, metersPerUnit=1`, re-orient converted assets, re-derive pseudo-loader rot args. Currently absent from every document. |
| 4 | **High** | Resolve S2: either schedule the OSG→USD bridge as a real milestone (acknowledging it links OSG) or re-point M4–M6 validation at a native-USD test sample; recommended: replace the bridge with the subset facade (§6.1). |
| 5 | **High** | Add R11 Windows: OpenUSD is MSVC-only; OMNeT++ ships MinGW. Decide (unsupported-3D-on-Windows / MinGW port attempt / MSVC sub-build) before promising "Linux/macOS/Windows". |
| 6 | **High** | Add lighting (`SetLightingState`/headlight) to M3 and the parity checklist. |
| 7 | **High** | NED/ini compatibility: ship deprecated alias NED modules so `IntegratedOsgVisualizer` etc. keep resolving; the showcases' ini files contradict the "configs keep working" promise otherwise. |
| 8 | **Medium** | Plan the `usd.msg`/descriptor replacement (inspector Fields tree + the `Register_Enum`s that currently live in `osg.msg`). |
| 9 | **Medium** | Fix multi-viewer plan: set `Qt::AA_ShareOpenGLContexts` in qtenv (absent today) or use one engine per viewer; make this a spike success criterion. |
| 10 | **Medium** | Specify USD runtime packaging honestly: `lib/usd` resource tree + TBB/MaterialX/OpenSubdiv deps; re-verify the ~30 MB size claim; define `PXR_PLUGINPATH`/rpath layout for the runtime-loaded plugin; add the `WITH_USD` static-link fragment to M1. |

Remaining Low items (fix opportunistically): include-site count (88, not ~250); file count
(90, not 84); HeartBeat "same class" wording; `LineWidth` px-vs-world row to ⚠️;
instancing-aware picking note; PROJ axis order; sRGB/color policy; manual/doc rewrite scope;
`-losgAnimation` makefrag + embedded-animation asset check; cite the Autodesk USD text
proposal as a watch-item.

---

## 8. Sources consulted

- OpenUSD `UsdImagingGLEngine` API: https://openusd.org/dev/api/class_usd_imaging_g_l_engine.html
- OpenUSD 25.11 announcement (Hydra 2 default): https://aousd.org/blog/announcing-openusd-v25-11-key-features-and-improvements/
- OpenUSD 26.03 announcement (targeted mesh invalidation): https://aousd.org/blog/openusd-v26-03/
- HgiGL GL 4.5 requirement: https://github.com/PixarAnimationStudios/OpenUSD/issues/2756
- OpenUSD supported toolchains (VERSIONS.md — MSVC-only on Windows): https://github.com/PixarAnimationStudios/OpenUSD/blob/release/VERSIONS.md
- USD text proposal (ASWF WG): https://forum.aousd.org/t/usd-text-proposal-presentation-in-todays-aswf-usd-working-group-call/1317
- guc: https://github.com/pablode/guc · Adobe plugins: https://github.com/adobe/USD-Fileformat-plugins · osgVerse: https://github.com/xarray/osgverse
- All file:line references verified directly in `~/DEV/omnetpp-6.4.0aipre2` and `samples/inet-4.6.0` on 2026-06-11.
