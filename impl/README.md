# Increment 1 — OSG → USD Migration: Staged Implementation Artifacts

This directory contains everything produced for **Increment 1** of the
OSG-to-OpenUSD migration of the OMNeT++ 3D visualization stack. Nothing here
has been applied to the real OMNeT++ tree yet, and **nothing here has been
compiled** — see the *Validated vs. Gated* section at the bottom.

Increment 1 delivers:

- **M0 (neutral kernel API):** the renderer-neutral `cScene3DNode` handle
  (`include/omnetpp/scene3dnode.h`, alias for `osg::Node` under `WITH_OSG`,
  opaque class otherwise), new `cEnvir::refSceneNode`/`unrefSceneNode` virtuals
  with no-op/forwarding defaults, and the retyping of `cOsgCanvas` from
  `osg::Node*` to `cScene3DNode*` — all designed to be ABI/API-identical for
  existing `WITH_OSG` consumers.
- **M1 (build plumbing):** `WITH_USD`/`WITH_USD_GEO` configure flags
  (default `no`; a `no` build is byte-identical to today), `USD_CFLAGS`/
  `USD_LIBS`/`PROJ_CFLAGS`/`PROJ_LIBS` wiring through `configure.in`,
  `Makefile.inc.in`, `config.h.in`, and the `src/qtenv/usd/` plugin Makefile
  producing **`oppqtenv-usd`** (name verified identical across the loader,
  the qtenv Makefile, and the plugin Makefile).
- **M2 (plugin skeleton):** `UsdViewer` (implements the existing
  `IOsgViewer`/`IOsgViewerFactory` seam; placeholder paintEvent), `StageCache`
  (Z-up, metersPerUnit=1 stage + fully implemented SdfPath→cObject pick
  registry), `CameraController` base + `TrackballController` stub, `usd.msg`
  inspector descriptors (mirrors `osg.msg`, minus OSG node-tree browsing,
  plus the two missing camera-manipulator enumerators), and the
  preference-ordered loader change in `iosgviewer.cc` (USD first, OSG
  fallback).
- **INET groundwork (M7 skeleton):** `inet/visualizer/usd/util/`
  (`UsdScene`, `UsdUtils`) mirroring `OsgScene`/`OsgUtils` function-for-
  function, with implemented trig/resource helpers and throwing stubs for
  everything that needs a stage-authoring pass; plus the `VisualizationUsd`
  `.oppfeatures` entry (initiallyEnabled=false).
- **Spike:** a standalone Qt6 + `UsdImagingGLEngine` de-risking app — **the
  gate for everything else**.

---

## Layout of `impl/`

```
impl/
├── README.md            ← this file
├── STATUS.md            ← per-file status table
├── tree/                ← NEW files; copy onto the OMNeT++ root verbatim
│   ├── include/omnetpp/scene3dnode.h
│   ├── src/qtenv/usd/                       (the oppqtenv-usd plugin)
│   │   ├── Makefile
│   │   ├── usdviewer.{h,cc}
│   │   ├── stagecache.{h,cc}
│   │   ├── cameracontrollers.{h,cc}
│   │   └── usd.msg
│   └── samples/inet-4.6.0/src/inet/visualizer/usd/util/
│       ├── UsdScene.{h,cc}
│       └── UsdUtils.{h,cc}
├── changes/             ← authoritative edit notes for EXISTING files
│   ├── m0-cenvir-refscenenode.md            (cenvir.h, cnullenvir.h, envirbase.h)
│   ├── m0-cosgcanvas-neutral.md             (cosgcanvas.h, cosgcanvas.cc)
│   ├── m1-configure-usd.md                  (configure.user, configure.in, config.h.in)
│   ├── m1-makefileinc-qtenv.md              (Makefile.inc.in, src/qtenv/Makefile)
│   ├── m2-iosgviewer-loader.md              (src/qtenv/iosgviewer.cc)
│   └── inet-oppfeatures-visualizationusd.md (samples/inet-4.6.0/.oppfeatures)
└── spike/               ← standalone de-risking app (throwaway; NOT shipped)
    ├── CMakeLists.txt
    ├── main.cpp
    └── README.md        (build/run instructions + success checklist)
```

There is no `patches/` directory in Increment 1; the `changes/*.md` notes are
the authoritative edit instructions (each contains exact anchors quoted from
the real source files, verified 2026-06-11, plus before/after text and a
unified diff where applicable).

---

## How to apply

> Do **not** apply until the spike has passed (see next section).

1. **Copy the new files** over the OMNeT++ root:

   ```bash
   cp -R impl/tree/* /Users/gabortabi/DEV/omnetpp-6.4.0aipre2/
   ```

   This adds only new files; it overwrites nothing.

2. **Apply the `changes/*.md` edits by hand**, in this order (each note lists
   exact line anchors and before/after text):

   1. `m0-cenvir-refscenenode.md`   — must land before m0-cosgcanvas-neutral
   2. `m0-cosgcanvas-neutral.md`
   3. `m1-configure-usd.md`         — then re-run autoconf (`./configure` regen)
   4. `m1-makefileinc-qtenv.md`     — mind the TABs in make recipe lines
   5. `m2-iosgviewer-loader.md`
   6. `inet-oppfeatures-visualizationusd.md` (INET, optional for M0–M2 builds)

3. **Build order / verification:**
   - First build with `WITH_USD=no` (the default): must be behaviorally
     identical to today (M0/M1 acceptance).
   - Then `WITH_USD=yes` with `USD_CFLAGS`/`USD_LIBS` pointing at your OpenUSD
     install: `make` should produce `lib/liboppqtenv-usd.*` and Qtenv should
     print `Loading 3D viewer library 'oppqtenv-usd'...` and show the
     skeleton placeholder in 3D inspectors.

---

## How to build OpenUSD minimally

Only the imaging stack is needed. Pin to **v25.11**. Monolithic `usd_ms`;
Python, tests, tutorials, examples and usdview all OFF. The imaging config
brings in `usdImaging`, `usdImagingGL`, `hdSt` (Storm), `hgiGL` (Linux),
`hgiMetal` + `hgiInterop` (macOS — GL is capped at 4.1 there, so Metal does
the rendering and hgiInterop blits into the GL FBO).

```bash
git clone https://github.com/PixarAnimationStudios/OpenUSD.git
cd OpenUSD && git checkout v25.11
mkdir build && cd build

cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=~/usd-install \
  -DPXR_BUILD_IMAGING=ON \
  -DPXR_BUILD_USD_IMAGING=ON \
  -DPXR_BUILD_MONOLITHIC=ON \
  -DPXR_ENABLE_PYTHON_SUPPORT=OFF \
  -DPXR_BUILD_TESTS=OFF \
  -DPXR_BUILD_EXAMPLES=OFF \
  -DPXR_BUILD_TUTORIALS=OFF \
  -DPXR_BUILD_USDVIEW=OFF \
  -DPXR_BUILD_ALEMBIC_PLUGIN=OFF \
  -DPXR_BUILD_OPENVDB_PLUGIN=OFF \
  -DPXR_ENABLE_OCIO_SUPPORT=OFF
# macOS: add  -DPXR_ENABLE_METAL_SUPPORT=ON   (enables hgiMetal + hgiInterop)

cmake --build . --parallel && cmake --install .
```

Result: `lib/libusd_ms.{so,dylib}`, `lib/cmake/pxr/pxrConfig.cmake`,
`include/pxr/`. `PXR_BUILD_IMAGING`/`PXR_BUILD_USD_IMAGING` pull in
hdSt/hgiGL automatically on Linux; Metal support flag adds hgiMetal/hgiInterop
on macOS. Windows is deferred — Windows users build/run under WSL2/WSLg
(Linux/HgiGL path); see `spike/README.md` §5.

---

## Build + run the SPIKE first — it is the gate

The spike (`impl/spike/`) is a standalone Qt6 app that exercises the exact
Qt/Hydra integration the plugin will use, **before** any of the staged changes
touch the OMNeT++ tree. Do not wire the plugin (M3) until the spike's
five-item success checklist passes on your target platform(s).

```bash
cd impl/spike && mkdir build && cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -Dpxr_DIR=~/usd-install/lib/cmake/pxr \
  -DCMAKE_PREFIX_PATH=<Qt6-install>
cmake --build . --parallel

# run (Linux)
LD_LIBRARY_PATH=~/usd-install/lib:$LD_LIBRARY_PATH ./usd_qt_spike
# run (macOS)
DYLD_LIBRARY_PATH=~/usd-install/lib ./usd_qt_spike
```

What it must prove (full checklist in `spike/README.md` §4):
1. Lit sphere renders — HgiGL on Linux / HgiMetal+hgiInterop on macOS (R3).
2. Click-picking returns the correct `SdfPath` (R2).
3. QPainter HUD overlay is uncorrupted (default or `--safe-overlay`).
4. The GL ≥ 4.5 version gate fires correctly when forced low (Linux).
5. `--two`: two viewers share one `Hgi` via `Qt::AA_ShareOpenGLContexts`.

The spike also carries an **API deviation log** (`spike/README.md` §6): every
OpenUSD 25.11 API call that was written from documentation, not against a
compiler, is listed there and must be confirmed or corrected during the first
real build. Expect to touch `main.cpp` for some of them
(`UsdImagingGLEngine::Parameters::driver`, `SetPresentationOutput`,
`CameraUtilFraming` ctor, `TestIntersection` signature, etc.).

---

## VALIDATED vs. GATED

**Nothing in this increment has been compiled.** "Validated" below means
*verified by inspection against the real source trees* (anchors, signatures,
naming, ABI arguments) — not by a build.

### Validated by inspection (2026-06-11)
- All `changes/*.md` line anchors match the real OMNeT++ 6.4.0aipre2 files
  (`cenvir.h` 844–854, `cnullenvir.h` 155–158, `envirbase.h` 281–284,
  `configure.in`/`configure.user`/`Makefile.inc.in` anchors, `osg.msg`).
- `cScene3DNode` alias direction and the `WITH_OSG` ABI-identity argument.
- Plugin library name **`oppqtenv-usd`** identical across loader, qtenv
  Makefile, and plugin Makefile.
- `usd.msg` mirrors `osg.msg` descriptors (and the `Register_Enum`-before-
  include ordering matches the working `osg.msg` exactly); it deliberately
  adds CAM_TERRAIN/CAM_OVERVIEW which `osg.msg` omits, and deliberately drops
  the OSG node-tree descriptors and `osg::Node *scene` field.
- `UsdUtils`/`UsdScene` map function-for-function onto `OsgUtils`/`OsgScene`.
- `UsdViewer` overrides every `IOsgViewer` pure virtual; signal re-declaration
  matches the existing `DummyOsgViewer` pattern.
- The `WITH_USD=no` no-op argument for configure/Makefile changes.

### Gated on a real OpenUSD build (first actual validation steps, in order)
1. **The spike** — compiles, runs, and its 5-item checklist + API deviation
   log pass on Linux and macOS. *Everything else is downstream of this.*
2. **M0/M1 build, `WITH_USD=no`** — apply M0+M1 changes, full OMNeT++ build,
   confirm byte/behavior-identical output and that the OSG path still works.
3. **M1 build, `WITH_USD=yes`** — `configure` finds usd_ms (validates the
   `OPP_CHECK_LIB` probes and header paths assumed in `m1-configure-usd.md`);
   `src/qtenv/usd/` compiles (validates pxr includes in `stagecache.h`,
   msgc on `usd.msg`, moc under `QT_NO_KEYWORDS`, plugin link line incl.
   macOS Metal frameworks).
4. **M2 runtime** — Qtenv loads `oppqtenv-usd`, placeholder shows, OSG
   fallback still loads when USD lib is absent.
5. **INET skeleton compile** — `opp_featuretool enable VisualizationUsd`,
   `util/` compiles against pxr headers (note: the `inet.visualizer.usd` NED
   package referenced by `.oppfeatures` has no `.ned` files yet — known
   transitional state, harmless while disabled).

### Known deferred items (by design, not bugs)
- `TopLevelUsdScene::getSimulationScene(cModule*)` throws — blocked on M4
  (USD-aware `refSceneNode` semantics / parallel canvas storage).
- All `UsdUtils` prim-authoring bodies throw — M7, with the intended
  implementation recorded inline.
- `Qt::AA_ShareOpenGLContexts` in Qtenv startup — M3 (the spike proves it).
- Terrain/Overview camera controllers — M5; dashed/dotted line styles — M8.
- Native Windows — deferred; WSL2/WSLg is the supported route (Decision Q9).
