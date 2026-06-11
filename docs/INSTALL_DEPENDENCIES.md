# Install Dependencies — OpenUSD 3D Visualization (OSG/osgEarth replacement)

What must be installed to **build and run** the OpenUSD/Hydra 3D visualization that replaces
OpenSceneGraph + osgEarth in OMNeT++/INET. This is written to fold into OMNeT++'s installation
process — i.e. the `configure`/`setenv` flow and the platform install guides should eventually
detect/require these, replacing the old OSG/osgEarth requirements.

> **Net effect on the OMNeT++ dependency list:** *remove* OpenSceneGraph (`-losg -losgDB
> -losgGA -losgViewer -losgUtil -lOpenThreads`) and osgEarth (`-losgEarth -losgEarthUtil`);
> *add* OpenUSD (imaging-enabled) and, for the geospatial layer, PROJ. Qt and the C++ toolchain
> are already OMNeT++ requirements.

---

## 1. Dependency summary

| Dependency | Min version | Role | macOS | Linux / WSL2 | Replaces |
|---|---|---|---|---|---|
| C++ toolchain (clang/gcc), C++17 | — | build | Xcode CLT | gcc/clang | (already req.) |
| CMake | 3.26+ | build USD; spike | brew/`cmake` | distro/`cmake` | (new for USD) |
| Python 3 | 3.8+ | runs USD `build_usd.py` | system/brew | distro | (build-time only) |
| git | any | fetch USD + deps | brew/Xcode | distro | (already req.) |
| **OpenUSD (imaging)** | **25.11** | **the renderer** | **build from source** | build / vcpkg / conan | **OSG + osgEarth** |
| **Qt** | **6.2+** | viewer widget + spike | `brew install qt` | distro `qt6-*` | (Qtenv already uses Qt) |
| PROJ | 9.x | geospatial coord conv. (M10, `WITH_USD_GEO`) | `brew install proj` | `libproj-dev` | osgEarth SRS math |

**Required OpenUSD components (must be built/enabled):** `usd`, `usdGeom`, `usdShade`,
`usdImaging`, `usdImagingGL`, `hd`, `hdSt` (Storm), `hgi`, **`hgiGL`** (Linux/Windows),
**`hgiMetal`** (macOS), **`hgiInterop`**, `glf`, `cameraUtil`, `gf`, `vt`, `sdf`, `tf`.
Built **monolithic** (`usd_ms`). Python bindings and `usdview` are **not** required by the
renderer (build with `--no-python`).

---

## 2. Platform notes

- **macOS (Apple Silicon & Intel):** macOS caps OpenGL at 4.1, but Hydra Storm's HgiGL needs
  ≥ 4.5 → the renderer uses **HgiMetal + hgiInterop** (the path `usdview` uses on macOS).
  OpenUSD must therefore be built **with Metal support** (the default on macOS).
- **Linux:** Storm via **HgiGL**, requires **OpenGL ≥ 4.5** (Mesa or vendor driver).
- **Windows:** native support is deferred (decision Q9). Windows users run under **WSL2** with
  **WSLg** providing the GL surface — i.e. the Linux/HgiGL path. (Native MSVC Windows is a
  later phase; note USD is MSVC-built while OMNeT++ ships MinGW — ABI-incompatible.)

---

## 3. OpenUSD — how to obtain it

There is currently **no Homebrew formula** for OpenUSD (verified: `brew info openusd` and
`brew info usd` both empty as of 2026-06), and the PyPI `usd-core` wheel ships **core only**
(no imaging/Hydra) — so it cannot drive the viewer. **Build from source.**

### Build command used on this machine (macOS arm64)
```bash
git clone --depth 1 --branch v25.11 \
    https://github.com/PixarAnimationStudios/OpenUSD.git ~/src/OpenUSD

python3 ~/src/OpenUSD/build_scripts/build_usd.py \
    --build-monolithic \
    --no-python --no-examples --no-tutorials --no-tests --no-docs \
    ~/USD
```
`build_usd.py` fetches and builds the third-party deps (TBB, OpenSubdiv, etc.) and installs
everything under the prefix (`~/USD`): `include/`, `lib/` (incl. `libusd_ms`), `plugin/`,
`bin/`. Imaging and usd-imaging are **on by default**; on macOS this yields the Metal Hgi.

**Production build flags to add later (not needed for the de-risking spike):**
- `--materialx` — required for the signal-propagation shader port (milestone M9).
- `--openimageio` — broader texture-format support for node icons/textures.
- (drop `--no-python` if `usdview` or Python asset tooling is wanted.)

### Environment (so OMNeT++/CMake/runtime find USD)
```bash
export USD_ROOT="$HOME/USD"
export PATH="$USD_ROOT/bin:$PATH"
export CMAKE_PREFIX_PATH="$USD_ROOT:$CMAKE_PREFIX_PATH"   # find_package(pxr CONFIG)
export DYLD_LIBRARY_PATH="$USD_ROOT/lib:$DYLD_LIBRARY_PATH"   # macOS runtime
# Linux: export LD_LIBRARY_PATH="$USD_ROOT/lib:$LD_LIBRARY_PATH"
# USD plugins are discovered from $USD_ROOT/lib/usd via the install's plugInfo;
# set PXR_PLUGINPATH_NAME only if relocating plugins.
```
These belong in OMNeT++'s `setenv` once `WITH_USD` is wired (replacing the OSG lib paths).

---

## 4. Folding into OMNeT++'s `configure` (replacing the OSG block)

`configure.in` should gain a `WITH_USD` detection block (and `WITH_USD_GEO` for PROJ),
mirroring the existing `WITH_OSG`/`WITH_OSGEARTH` blocks (see
[`docs/01-dependency-inventory.md`](01-dependency-inventory.md) §1.3 and the staged change-note
`impl/changes/m1-configure-usd.md`):

- Probe `pxr/usd/usd/stage.h` + `pxr/usdImaging/usdImagingGL/engine.h`, link `-lusd_ms`,
  check `PXR_VERSION >= 2511`; set `USD_CFLAGS`/`USD_LIBS`; `AC_DEFINE(WITH_USD)`.
- Probe `proj.h` + `-lproj`; `AC_DEFINE(WITH_USD_GEO)`.
- Once USD is the default, the OSG/osgEarth detection and the `-losg…` link fragments are
  **removed** (milestone M12).

---

## 5. What was installed on this machine

Recorded 2026-06-11 on this host (macOS 26.4.1, Apple Silicon arm64).

- **Present already (no action):** Xcode CLT (Apple clang 21), Homebrew 5.1.14, CMake 4.0.3,
  Python 3.12.1, git 2.50.0, **Qt 6.11.1** (Homebrew, at `/opt/homebrew/opt/qt`).
- **Installed by this work:** OpenUSD **v25.11** (`PXR_VERSION 2511`, commit `363a7c8`) — built
  from source to `~/USD`, monolithic, imaging on, **Metal (hgiMetal) present**, `--no-python`.
  Build time ≈ 9 min on an M3-class machine. **MaterialX, OpenSubdiv and TBB were built in by
  default** — so the M9 shader work does **not** need a USD rebuild after all.
- **Deferred (install when the milestone needs it):** PROJ 9.x (geospatial, M10); osgVerse +
  Adobe `usdGLTF` for the `.osgb`→`.usd` asset pipeline (M7, build-time tools only).
  OpenImageIO is OFF in this build — add `--openimageio` later if exotic texture formats are
  needed (PNG/JPG work via USD's built-in Hio).

```
OpenUSD version : v25.11  (PXR_VERSION 2511, commit 363a7c8)
Install prefix  : /Users/gabortabi/USD            (~1.1 GB)
Monolithic lib  : ~/USD/lib/libusd_ms.dylib       (68 MB)
Imaging         : include/pxr/usdImaging/usdImagingGL/engine.h ; hgiMetal present
Extras built in : MaterialX, OpenSubdiv, TBB
CMake config    : ~/USD/pxrConfig.cmake           (find_package(pxr CONFIG))
Tools           : ~/USD/bin (usdcat, usdchecker, usdtree, …)
Qt              : /opt/homebrew/opt/qt            (qt 6.11.1)
```

### Verified on this host
The de-risking spike (`impl/spike/`) **configures, compiles, links, and loads** against this
install — `cmake` found `pxr` + Qt6 and auto-selected the **macOS HgiMetal** path; the binary
links `@rpath/libusd_ms.dylib` + the Qt frameworks; and `usd_qt_spike --help` runs (resolving
every USD/Qt dylib at load and constructing `QApplication`) and exits 0. This validates the
dependency set and the C++/CMake/runtime-link chain end to end.

Three API-surface corrections were needed in the spike during this bring-up (now fixed and
logged in `impl/spike/main.cpp`): (1) `pxr/base/gf/range2i.h` does not exist — `GfRange2i` is
not a USD type (USD has `range{1,2,3}{d,f}` + `rect2i`); (2)/(3) missing Qt includes
(`QtGui/QOpenGLContext`, `QtGui/QImage`, `QtGui/QFont`, `QtCore/QCommandLineOption`). Every
other API assumption in the spike's deviation log (CreatePlatformDefaultHgi, HgiTokens,
`Parameters::driver`, `SetPresentationOutput`/`SetFraming`/`SetRenderBufferSize`/
`SetLightingState`/`TestIntersection`/`IsConverged`, `CameraUtilFraming(GfRect2i)`) **matches
OpenUSD 25.11** as verified against the installed headers.

**Runtime note for `setenv`:** the binary references `@rpath/libusd_ms.dylib`, so the loader
needs `DYLD_LIBRARY_PATH=$USD_ROOT/lib` (macOS) / `LD_LIBRARY_PATH=$USD_ROOT/lib` (Linux) — see
§3. (Alternatively bake an absolute rpath at link time.)

**Still requires an interactive GUI session (not done here):** the spike's *rendering* checklist
— lit sphere via HgiMetal, `TestIntersection` picking, QPainter/QImage overlay, two-viewer
shared-`Hgi` — needs a real display/Metal context. That run is the **M3 go/no-go gate** and
should be done on the developer's desktop or a CI runner with a GPU/display.

---

## 6. Verifying the install (the de-risking spike)

The standalone spike in [`../impl/spike/`](../impl/spike/) is the first consumer and the
**M3 go/no-go gate**. With the environment from §3 set:
```bash
cmake -S impl/spike -B impl/spike/build -DCMAKE_PREFIX_PATH="$USD_ROOT;$(brew --prefix qt)"
cmake --build impl/spike/build
./impl/spike/build/usd_qt_spike            # GUI; --safe-overlay for the QImage text path
```
Success criteria are in `impl/spike/README.md` (renders a lit sphere via HgiMetal/HgiGL,
picking returns a prim path, overlay text is uncorrupted, GL/Metal backend selected, two
viewers share one `Hgi`). The spike also confirms/corrects the **9 API-surface assumptions**
logged in `impl/spike/main.cpp` against the real OpenUSD version built here.
