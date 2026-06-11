# Applied to the live OMNeT++ tree (2026-06-11)

The M0–M2(+) implementation is **live** in `/Users/gabortabi/DEV/omnetpp-6.4.0aipre2`
(which is *not* under git). Backups of every modified pre-existing file are in
`<tree>/.usd-migration-backups/`. `impl/tree/` in this repo mirrors the **as-built,
authoritative** versions of all new files.

## In-tree modifications (backed up)

| File | Change |
|---|---|
| `include/omnetpp/scene3dnode.h` | **NEW** — `cScene3DNode` alias (`= osg::Node` under `WITH_OSG`) / opaque class otherwise |
| `include/omnetpp/cosgcanvas.h` | scene root `osg::Node*` → `cScene3DNode*` (field, ctor, `setScene`/`getScene`) |
| `src/sim/cosgcanvas.cc` | ref/unref helpers retyped; route through `cEnvir::refSceneNode/unrefSceneNode` |
| `include/omnetpp/cenvir.h` | + non-pure `refSceneNode/unrefSceneNode` forwarding to deprecated `refOsgNode/unrefOsgNode` |
| `include/omnetpp/cnullenvir.h` | + no-op overrides |
| `src/envir/envirbase.h` | **deliberately NOT overridden** (a no-op here would shadow the forwarding default and break Qtenv's ref path — fixed a bug in the original change note) |
| `src/qtenv/iosgviewer.cc` | loader prefers `oppqtenv-usd`, falls back to `oppqtenv-osg`, then dummy; **`dladdr`-based absolute-path fallback** (bare-name `dlopen` never worked on macOS without `DYLD_LIBRARY_PATH` — pre-existing bug, also fixes OSG loading) |
| `src/qtenv/Makefile` | + `qtenv-usd` subdir gate on `WITH_USD=yes` |
| `Makefile.inc` | `WITH_OSG=no`; + `WITH_USD=yes`, `USD_ROOT`, `USD_CFLAGS` (**`-isystem`**), `USD_LIBS` (with `-rpath`) — hand-wired pending configure.in integration |
| `include/omnetpp/platdep/config.h` | `WITH_OSG` undefined; `WITH_USD` defined — hand-wired pending configure |
| `lib/liboppqtenv-osg*.dylib` | moved to backups (quarantined) |

## New plugin: `src/qtenv/usd/` → `liboppqtenv-usd.dylib`

`usdviewer.{h,cc}` (UsdViewer : IOsgViewer — engine, camera orbit/zoom, picking via
`TestIntersection` → `cScene3DNode` registry, hints, heartbeat; Linux HgiGL `paintGL`
path written but untested), `usdpresent_metal.mm` (the validated spike recipe: CAMetalLayer
sublayer on a mouse-transparent native child widget, render-buffer texture, commit +
synchronizing readback, frame pacing), `usdscenehandle.h` (completes `cScene3DNode`:
UsdStage + prim→cObject pick registry + intrusive refcount), `Makefile`.

## New sample: `samples/usd-intro`

4-node token-ring simulation; `UsdScene` module authors a Z-up/meters stage (ground mesh,
4 node spheres bound to their `TokenNode` modules for picking, token sphere, sim-time-driven
orbiter), updates it from the `tokenHop` signal, hands it over via
`getOsgCanvas()->setScene(handle)`. Built with `opp_makemake -i makefrag` (USD flags).
**Verified: builds; runs headless in Cmdenv; Qtenv loads `oppqtenv-usd`.**

## Toolchain facts discovered (recorded in docs/INSTALL_DEPENDENCIES.md)

1. **TBB header patch required** with newer clang: `~/USD/include/tbb/task.h` `kind_type`
   enum widened (`_kind_type_range_max = 7`) — clang ≥ 20 makes out-of-range enum constants
   a **non-downgradable hard error**; upstream oneTBB fixed it the same way. (`task.h.orig`
   kept beside it.) This machine's OMNeT++ builds with **Homebrew LLVM 22** (PATH), while
   the spike used Apple clang 21 (accepts it) — hence the spike didn't hit it.
2. **`-isystem` (not `-I`) for USD includes**, and **`-std=gnu++17`** appended (TBB needs
   GNU extensions on strict compilers).
3. Sample binaries get the USD rpath via `USD_LIBS`, so no `DYLD_LIBRARY_PATH` is needed.

## DEMO VERIFIED (2026-06-11, user-confirmed screenshot)

The `usd-intro` simulation **renders and runs live in Qtenv on macOS**: ground plane,
4 ring nodes with the active node recolored green on token arrival, yellow token sphere,
red sim-time-driven orbiter — all updating while the simulation executes (confirmed at
event #2907, t=581s). 3D picking selects the bound `TokenNode` modules (reliability
improved post-demo by widening the pick window from 1px to 6px — Risk R2 behavior).

**Additional USD 25.11 gotcha discovered:** `UsdImagingGLEngine::GetHgi()` returns
**null** when the Hgi is supplied via `HdDriver` — the presenter must keep its own
reference to the shared Hgi. (Found via qInfo breadcrumbs; qDebug is compiled out in
release Qtenv builds, and Qtenv appears to swallow qWarning — use qInfo for plugin
diagnostics.)

## Known transitional limitations

- `configure.in` not yet regenerated — `Makefile.inc`/`config.h` are hand-edited (M1's
  configure block remains to be landed when a full reconfigure is wanted).
- ABI note: adding `cEnvir` virtuals shifts vtables — **all** libs/samples built against
  the old headers must be recompiled (`liboppcmdenv` bit us; samples need `make clean`).
- OSG samples' 3D views are dummy now (expected; OSG removed). The `osg.msg` inspector
  descriptors are not yet ported to a `usd.msg` (Fields-tab browsing of the scene object).
- Camera manipulator hints (`CAM_*`) map to a single orbit controller for now.
