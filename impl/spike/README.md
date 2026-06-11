# usd_qt_spike — De-risking spike

Standalone Qt 6 + `UsdImagingGLEngine` application that validates the
highest-risk technical items before milestone M3:

| Risk | What is validated |
|------|-------------------|
| **R3** | Qt/Hydra GL context interop per platform: HgiGL on Linux, HgiMetal + hgiInterop on macOS; GL-version gate; `AA_ShareOpenGLContexts` + one-Hgi-many-views |
| **R2** | `TestIntersection` picking surfaces the correct `SdfPath` |
| **R11** | Windows = WSL2/WSLg; no native Windows code needed in the spike |

This is a throwaway validator.  It is not shipped.

---

## 1. Build OpenUSD minimally

Only the imaging stack is needed.  This produces the monolithic `usd_ms`
library (~30 MB) and avoids Python, tests, tutorials, examples, and usdview.

```bash
git clone https://github.com/PixarAnimationStudios/OpenUSD.git
cd OpenUSD
git checkout v25.11          # pin to 25.11 per docs/06 Q7
mkdir build && cd build

# --- Linux ---
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

# --- macOS (add Metal support) ---
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=~/usd-install \
  -DPXR_BUILD_IMAGING=ON \
  -DPXR_BUILD_USD_IMAGING=ON \
  -DPXR_BUILD_MONOLITHIC=ON \
  -DPXR_ENABLE_METAL_SUPPORT=ON \
  -DPXR_ENABLE_PYTHON_SUPPORT=OFF \
  -DPXR_BUILD_TESTS=OFF \
  -DPXR_BUILD_EXAMPLES=OFF \
  -DPXR_BUILD_TUTORIALS=OFF \
  -DPXR_BUILD_USDVIEW=OFF \
  -DPXR_BUILD_ALEMBIC_PLUGIN=OFF \
  -DPXR_BUILD_OPENVDB_PLUGIN=OFF \
  -DPXR_ENABLE_OCIO_SUPPORT=OFF

cmake --build . --parallel $(nproc)
cmake --install .
```

The install tree will contain:
- `lib/libusd_ms.so` (Linux) / `lib/libusd_ms.dylib` (macOS)
- `lib/cmake/pxr/pxrConfig.cmake` — the CMake config file this spike uses
- `include/pxr/` headers

---

## 2. Configure and build the spike

### Linux

```bash
cd /path/to/omnetpp-osg-replacement/impl/spike
mkdir build && cd build

cmake .. \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -Dpxr_DIR=~/usd-install/lib/cmake/pxr \
  -DCMAKE_PREFIX_PATH=~/Qt/6.x.x/gcc_64

cmake --build . --parallel $(nproc)
```

### macOS

```bash
cd /path/to/omnetpp-osg-replacement/impl/spike
mkdir build && cd build

cmake .. \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -Dpxr_DIR=~/usd-install/lib/cmake/pxr \
  -DCMAKE_PREFIX_PATH=~/Qt/6.x.x/macos \
  -DCMAKE_OSX_ARCHITECTURES=arm64   # or x86_64

cmake --build . --parallel $(nproc)
```

**Note:** On macOS, `PXR_ENABLE_METAL_SUPPORT=ON` must have been passed to the
OpenUSD build (step 1).  The resulting `libusd_ms.dylib` contains `hgiMetal`
and `hgiInterop`, so no extra link flags are needed in this spike.

---

## 3. Run the spike

### Basic run (single viewer)

```bash
# Linux (may need to set library path)
LD_LIBRARY_PATH=~/usd-install/lib:$LD_LIBRARY_PATH ./usd_qt_spike

# macOS
DYLD_LIBRARY_PATH=~/usd-install/lib ./usd_qt_spike
```

### Two-viewer shared-Hgi test

```bash
./usd_qt_spike --two
```

Both windows must render the same scene simultaneously.  This validates that
`Qt::AA_ShareOpenGLContexts` + a single `Hgi` instance (created in the first
`initializeGL()`) serves multiple `UsdImagingGLEngine` instances correctly.

### Safe overlay mode

```bash
./usd_qt_spike --safe-overlay
```

Uses the QImage-blit overlay path instead of direct `QPainter` text rendering.
Use this if the HUD text appears corrupted or blank (Hydra GL pixel-transfer
state leakage; see docs/02 §4.2).

### Force a low GL version to test the version gate (Linux only)

```bash
# With Mesa, pin to GL 3.3 to trigger the fatal diagnostic:
MESA_GL_VERSION_OVERRIDE=3.3 ./usd_qt_spike
# Expected output: FATAL: OpenGL >= 4.5 is required for HgiGL ...
```

---

## 4. Success checklist (maps to docs/06 section C)

Run through all five items on **both platforms** before declaring the spike
closed.  Record findings in section 5 below.

**1. Lit sphere renders on Linux/HgiGL and macOS/HgiMetal**
   - [ ] Blue sphere visible in the window with Phong shading (not flat/unlit).
   - [ ] Periwinkle background colour (#8080DB).
   - [ ] HUD line 1 shows the correct backend: `HgiGL` (Linux) or `HgiMetal`
         (macOS).
   - *Maps to R3 — platform-correct Hgi selection.*

**2. Picking reports the correct SdfPath (R2)**
   - [ ] Left-click on the large blue sphere → HUD shows `/World/Sphere`.
   - [ ] Left-click on the small yellow marker sphere → HUD shows `/World/Marker`.
   - [ ] Left-click on the background (miss) → HUD shows `(no hit)`.
   - [ ] Near-silhouette clicks on both spheres report the correct path.
         *(Near-silhouette picks are the hard case for the ID pass; R2 note.)*

**3. Overlay text uncorrupted**
   - [ ] HUD text is readable without garbage pixels in default mode.
   - [ ] HUD text is readable with `--safe-overlay`.
   - *Record which mode is required on which platform (section 5).*

**4. GL version gate fires correctly on Linux when forced low**
   - [ ] `MESA_GL_VERSION_OVERRIDE=3.3 ./usd_qt_spike` prints the fatal
         diagnostic and exits (does not crash silently or render garbage).

**5. `--two` shows two live viewers sharing one Hgi**
   - [ ] Both windows open and render the same scene simultaneously.
   - [ ] Clicking in one window updates its HUD independently of the other.
   - [ ] No crashes, GL errors, or black windows.
   - *Maps to R3 — AA_ShareOpenGLContexts + one-Hgi-many-views.*

---

## 5. Windows / WSL2 note (Risk R11)

Native Windows is deferred (Decision Q9, 2026-06-11).  OpenUSD is effectively
MSVC-built on Windows, while OMNeT++ ships a MinGW toolchain; the two C++ ABIs
are incompatible.

**Windows users run the Linux build under WSL2/WSLg.**  WSLg provides a GL
surface via `weston` + `wslg-d3d12`, which is sufficient for HgiGL (the Linux
path).  Steps:

1. Ensure WSL2 with WSLg is installed (Windows 11 or Windows 10 21H2+).
2. Follow the Linux build instructions above inside the WSL2 terminal.
3. Run `./usd_qt_spike` — WSLg forwards the X11/Wayland display automatically.

Native MSVC/MinGW Windows packaging is a later phase and is not on the
critical path for M3.

---

## 6. API deviation log (fill in after running)

Record any deviations from the OpenUSD 25.11 API surface assumed during
development.  This section gates the M3 milestone decision.

| Assumed API | Actual API (25.11) | Notes |
|-------------|-------------------|-------|
| `UsdImagingGLEngine::Parameters::driver` member | *to be verified* | Fallback: use `UsdImagingGLEngine(const HdDriver&, ...)` constructor overload |
| `Hgi::CreatePlatformDefaultHgi()` | *to be verified* | May be `Hgi::GetPlatformDefaultHgi()` or factory function in some builds |
| `HgiTokens->renderDriver` | *to be verified* | Check `pxr/imaging/hgi/tokens.h` |
| `HgiTokens->OpenGL` (present output token) | *to be verified* | May be `TfToken("OpenGL")` if not in tokens.h |
| `engine->GetCurrentRendererId()` | *to be verified* | Returns `TfToken`; may differ in name |
| `GlfSimpleLight::SetPosition(GfVec4f)` | *to be verified* | Check `pxr/imaging/glf/simpleLight.h` |
| `CameraUtilFraming(GfRect2i(...))` | *to be verified* | Constructor signature in `cameraUtil/framing.h` |
| `GfFrustum::ComputeNarrowedFrustum(GfVec2d, GfVec2d)` | *to be verified* | Check parameter types in `gf/frustum.h` |
| `engine->TestIntersection(view, proj, root, rp, &pt, &nrm, &path, &instPath, &instIdx)` | *to be verified* | Signature in `usdImagingGL/engine.h` |

**Overlay mode result:**
- Linux: [ ] direct QPainter clean  /  [ ] requires --safe-overlay
- macOS: [ ] direct QPainter clean  /  [ ] requires --safe-overlay
