# Increment 1 — File Status

Reviewed + integrated 2026-06-11 (dev-manager pass). NEW = file to copy onto
the OMNeT++ root from `tree/`; MODIFY = change-note describing edits to an
existing file; SPIKE = standalone, not shipped. **No file has been compiled
yet** — see the Gated-on column and `README.md`.

| File | Kind | Milestone | Status | Gated on |
|---|---|---|---|---|
| `tree/include/omnetpp/scene3dnode.h` | NEW | M0 | Complete. Alias (`WITH_OSG`) / opaque class (`!WITH_OSG`); ABI-identity argument verified against cosgcanvas consumers. | M0/M1 build (`WITH_USD=no` first) |
| `changes/m0-cenvir-refscenenode.md` | MODIFY (cenvir.h, cnullenvir.h, envirbase.h) | M0 | Complete. Anchors verified against real files; ref/unrefSceneNode signatures consistent with all callers. | M0/M1 build |
| `changes/m0-cosgcanvas-neutral.md` | MODIFY (cosgcanvas.h, cosgcanvas.cc) | M0 | Complete. All 11 `osg::Node` occurrences accounted for; "opaque struct" wording fixed to "opaque class" in review. | M0/M1 build |
| `changes/m1-configure-usd.md` | MODIFY (configure.user, configure.in, config.h.in) | M1 | Complete. 6 surgical insertions, anchors verified; `WITH_USD=no` no-op argument sound. OPP_CHECK_LIB probe headers/exprs are doc-based assumptions. | USD build: probes (PXR_VERSION 2511, usdImagingGL header path, `-lusd_ms` link) |
| `changes/m1-makefileinc-qtenv.md` | MODIFY (Makefile.inc.in, src/qtenv/Makefile) | M1 | Complete. 7 insertions; mirrors OSG blocks char-for-char; correct KERNEL_LIBS exclusion rationale; TAB warnings included. | M1 build both `WITH_USD` settings |
| `changes/m2-iosgviewer-loader.md` | MODIFY (src/qtenv/iosgviewer.cc) | M2 | Complete. USD-first preference loop with OSG fallback; lib name `oppqtenv-usd` matches plugin Makefile; behavioral-equivalence argument for OSG-only builds holds. | M2 runtime test |
| `changes/inet-oppfeatures-visualizationusd.md` | MODIFY (samples/inet-4.6.0/.oppfeatures) | M7 (groundwork) | Complete. Mirrors VisualizationOsg; initiallyEnabled=false. NED package `inet.visualizer.usd` has no .ned files yet (documented transitional state). | INET skeleton compile; M7 NED files |
| `tree/src/qtenv/usd/Makefile` | NEW | M1 | Complete. Mirror of osg/Makefile; LIBNAME `oppqtenv-usd` consistent; QT_NO_KEYWORDS + macOS Metal frameworks present. Vestigial icons-prereq line copied from osg (harmless, RESOURCES empty). | USD build (msgc/moc/link of plugin) |
| `tree/src/qtenv/usd/usdviewer.h` | NEW | M2 | Complete skeleton. All IOsgViewer pure virtuals overridden; slot/signal layout matches DummyOsgViewer pattern; M3–M6 TODOs annotated. | USD build + M2 runtime |
| `tree/src/qtenv/usd/usdviewer.cc` | NEW | M2 | Complete skeleton. Self-registering factory mirrors RealOsgViewerFactory. **Fixed in review:** duplicate out-of-line `getOsgCanvas()` definition removed (was also inline in header — redefinition error). | USD build + M2 runtime |
| `tree/src/qtenv/usd/stagecache.h` | NEW | M2 (registry) / M4 (builder) | Complete for M2. Registry fully designed; stage authored Z-up, m/unit=1. Wording fix: opaque *class*. | USD build (pxr headers); M4 scene builder |
| `tree/src/qtenv/usd/stagecache.cc` | NEW | M2/M4 | Registry fully implemented (componentId-vs-pointer storage mirrors cObjectOsgNode); getOrBuildStage returns empty stage until M4. | USD build; M4 |
| `tree/src/qtenv/usd/cameracontrollers.h` | NEW | M2 (base) / M3+M5 | Base class + Trackball stub complete; Terrain/Overview are M5 declarations-as-comments. | USD build; M3 orbit math |
| `tree/src/qtenv/usd/cameracontrollers.cc` | NEW | M2 | setViewpoint/getViewMatrix implemented (pure GfMatrix4d math). | USD build (SetLookAt API check) |
| `tree/src/qtenv/usd/usd.msg` | NEW | M2 | Complete. Mirrors osg.msg descriptors minus OSG node browsing; registers all 5 CameraManipulatorType enumerators (fixes osg.msg's silent omission of CAM_TERRAIN/CAM_OVERVIEW); cplusplus-block ordering identical to working osg.msg. | USD build (msgc generation + compile) |
| `tree/samples/.../usd/util/UsdScene.h` | NEW | M7 (skeleton) | Complete skeleton; mirrors OsgScene class-for-class without osg::Group/NodeVisitor. | USD build; M4 (cModule overload blocked, throws by design) |
| `tree/samples/.../usd/util/UsdScene.cc` | NEW | M7 (skeleton) | getStage/getSimulationScene implemented; static cModule* overload throws with full design rationale (M4 dependency). | USD build; M4 |
| `tree/samples/.../usd/util/UsdUtils.h` | NEW | M7 (skeleton) | Complete. Function-for-function mapping from OsgUtils.h verified; TextLabel/Billboard handle designs for the no-text/no-billboard USD gaps. | USD build (pxr headers) |
| `tree/samples/.../usd/util/UsdUtils.cc` | NEW | M7 (skeleton) | Trig helpers + resolveImageResource fully ported; 13 prim-authoring functions are documented throwing stubs (M7/M8). | USD build; M7 implementation |
| `spike/CMakeLists.txt` | SPIKE | pre-M3 gate | Complete. find_package(pxr CONFIG) + Qt6; QT_NO_KEYWORDS; HgiMetal/hgiInterop note for macOS. | OpenUSD 25.11 install (pxrConfig.cmake) |
| `spike/main.cpp` | SPIKE | pre-M3 gate | Complete. Hgi selection (CreatePlatformDefaultHgi), GL≥4.5 gate, shared-Hgi two-viewer mode, picking, overlay modes. **Fixed in review:** missing `<memory>`, `<cmath>`, `gf/vec2i.h`, `gf/rect2i.h` includes. 9 API-surface assumptions logged for verification. | First spike compile/run — THE gate |
| `spike/README.md` | SPIKE | pre-M3 gate | Complete. Minimal USD build recipe, per-platform run steps, 5-item success checklist, API deviation log, WSL2 note. | — |
| `README.md` | DOC | — | Written by this review pass. | — |
| `STATUS.md` | DOC | — | This file. | — |
