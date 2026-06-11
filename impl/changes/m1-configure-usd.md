# m1-configure-usd — Change Note

**Work item:** `m1-configure-usd`
**Milestone:** M1
**Kind:** MODIFY (surgical insertions — no existing lines removed or altered)
**Targets:** `configure.user`, `configure.in`, `include/omnetpp/platdep/config.h.in`

---

## Background

OMNeT++ 6.x already detects OpenSceneGraph (OSG) and osgEarth via `OPP_CHECK_LIB` in
`configure.in` (lines 1145–1180). The USD build support follows the same pattern exactly:
same macro, same flag variable conventions, same `AC_DEFINE` / `AC_SUBST` placement.

C++17 is already required and enforced by OMNeT++ 6.x — the check is in `configure.in`
at lines 278–295, and `configure.user` sets `CXXFLAGS=-std=c++17` at line 63. OpenUSD
requires C++17 as well, so no additional language-standard wiring is needed.

**Default is `WITH_USD=no`** for Increment 1 (opt-in until the viewer plugin exists in
M3). A `WITH_USD=no` build is byte-identical to today — no object files or `config.h`
entries change.

`USD_CFLAGS` / `USD_LIBS` are user-overridable in exactly the same way as
`OSG_CFLAGS` / `OSG_LIBS`: leave them unset and the defaults (`-lusd_ms`, `-lproj`)
apply; set them in `configure.user` to point at a non-system install.

---

## Change 1 — `configure.user`

**File:** `configure.user`
**Anchor:** after line 88 (`WITH_OSGEARTH=no`), before line 90 (`# Set to "yes" to enable the AI Chat…`)

**Surrounding context (from the real file):**
```
86:  #
87:  # Set to "yes" to enable osgEarth support in Qtenv (requires OpenScreenGraph enabled)
88:  #
89:  WITH_OSGEARTH=no
90:  
91:  #
92:  # Set to "yes" to enable the AI Chat in Qtenv (LLM-powered chat, MCP server,
```

**Insert after line 89 (`WITH_OSGEARTH=no`):**

```bash
#
# Set to "yes" to enable OpenUSD (Hydra/Storm) 3D rendering support in Qtenv.
# Requires OpenUSD 25.11 or later built with imaging (PXR_BUILD_IMAGING=ON,
# PXR_BUILD_USD_IMAGING=ON) and the monolithic usd_ms target.
# Use USD_CFLAGS and USD_LIBS to point at a non-system install.
#
WITH_USD=no

#
# Set to "yes" to enable the geospatial layer of the OpenUSD support.
# Requires PROJ 9.x (proj.h / -lproj) and requires WITH_USD=yes.
# Use PROJ_CFLAGS and PROJ_LIBS for a non-system install.
#
WITH_USD_GEO=no
```

---

## Change 2 — `configure.in`: USD/PROJ detection block

**File:** `configure.in`
**Anchor:** after line 1180 (`fi # WITH_OSG`), before line 1182 (`#----------------------`)

**Surrounding context (from the real file):**
```
1178:        fi
1179:      fi
1180: fi # WITH_OSG
1181: 
1182: #----------------------
1183: # Check for optional libraries allowing stack unwinding …
```

**Insert after line 1180 (`fi # WITH_OSG`):**

```sh
#-----------------------------
# Detecting OpenUSD (WITH_USD) and PROJ (WITH_USD_GEO)
#
# Platform notes (Decision D6 / Q9):
#   Linux  — HgiGL (OpenGL >= 4.5).  Build USD with PXR_BUILD_HGI_GL=ON.
#   macOS  — HgiMetal + hgiInterop.  OpenGL is capped at 4.1 on macOS; HgiGL
#             is not available.  Build USD with PXR_BUILD_HGI_METAL=ON and
#             ensure hgiInterop (the GL↔Metal blit bridge) is compiled into
#             the usd_ms monolithic target.  See the spike README for the
#             exact CMake invocation.
#   Windows — DECIDED (Q9, 2026-06-11): defer native Windows.  Windows users
#             run under WSL2 (the Linux/HgiGL path; WSLg provides the GL
#             surface), which sidesteps the MSVC-vs-MinGW ABI incompatibility.
#             Native MSVC/MinGW Windows packaging is a later phase.
#-----------------------------
USD_VERSIONCHECK_CODE=$(cat <<-EOT
#if PXR_VERSION < 2511
  #error Requires OpenUSD 25.11 or later
#else
  pxr::UsdStage::CreateInMemory();
#endif
EOT
)
if test "$WITH_USD" = "yes"; then
  USD_LIBS=${USD_LIBS:-"-lusd_ms"}
  OPP_CHECK_LIB(OpenUSD, pxr/usd/usd/stage.h, $USD_VERSIONCHECK_CODE, $CFLAGS $CFLAGS_ARCH $USD_CFLAGS $CFLAGS_TOOLS_MACOSX, $LDFLAGS $LDFLAGS_ARCH $LDFLAGS_TOOLS_MACOSX $USD_LIBS, usd_ok)
  if test $usd_ok = no; then
    AC_MSG_ERROR([Cannot find OpenUSD 25.11 or later (monolithic usd_ms build with imaging) - 3D view in Qtenv will not be available. Set WITH_USD=no in configure.user to disable this feature, or set USD_CFLAGS/USD_LIBS to point to your OpenUSD installation.])
  else
    # the imaging surface (UsdImagingGLEngine) must be present too
    OPP_CHECK_LIB(OpenUSD usdImagingGL, pxr/usdImaging/usdImagingGL/engine.h, pxr::UsdImagingGLEngine::GetRendererPlugins();, $CFLAGS $CFLAGS_ARCH $USD_CFLAGS $CFLAGS_TOOLS_MACOSX, $LDFLAGS $LDFLAGS_ARCH $LDFLAGS_TOOLS_MACOSX $USD_LIBS, usdimaging_ok)
    if test $usdimaging_ok = no; then
      AC_MSG_ERROR([OpenUSD found, but UsdImagingGLEngine is unavailable - build OpenUSD with PXR_BUILD_IMAGING=ON and PXR_BUILD_USD_IMAGING=ON (HgiGL on Linux, HgiMetal on macOS).])
    fi
    if test "$WITH_USD_GEO" = "yes"; then
      PROJ_LIBS=${PROJ_LIBS:-"-lproj"}
      OPP_CHECK_LIB(PROJ, proj.h, proj_info();, $CFLAGS $CFLAGS_ARCH $PROJ_CFLAGS $CFLAGS_TOOLS_MACOSX, $LDFLAGS $LDFLAGS_ARCH $LDFLAGS_TOOLS_MACOSX $PROJ_LIBS, proj_ok)
      if test $proj_ok = no; then
        AC_MSG_ERROR([Cannot find PROJ 9.x. Set WITH_USD_GEO=no in configure.user to disable the geospatial layer or install the PROJ development package.])
      fi
    fi # WITH_USD_GEO
  fi
fi # WITH_USD
```

### OPP_CHECK_LIB probe verification

`OPP_CHECK_LIB` compiles `#include <$2>` plus `$3;` as a C++ program and tries to link
it (via `AC_LINK_IFELSE` → `AC_LANG_PROGRAM`).  Verification of the three probes:

| Probe | Header | Body expression | Valid? |
|---|---|---|---|
| USD version + stage | `pxr/usd/usd/stage.h` | conditional `#error` / `pxr::UsdStage::CreateInMemory();` | Yes — `CreateInMemory()` is a `static UsdStageRefPtr` factory; the `#if PXR_VERSION` block guards it against too-old headers. The result is a temporary `UsdStageRefPtr` that is discarded; no `void`-return issue. |
| usdImagingGL | `pxr/usdImaging/usdImagingGL/engine.h` | `pxr::UsdImagingGLEngine::GetRendererPlugins();` | Yes — `GetRendererPlugins()` is a `static TfTokenVector` factory (returns by value, value is discarded as a statement). |
| PROJ | `proj.h` | `proj_info();` | Yes — `proj_info()` returns a `PJ_INFO` struct by value; calling it as a statement is valid C++. |

**Assumption:** OpenUSD 25.11 (PXR_VERSION 2511) uses the above headers and the static
factories behave as described.  This is based on the public OpenUSD GitHub tree.  If the
packaging puts `pxr/usdImaging/usdImagingGL/engine.h` at a different path, adjust the
probe header (not the library name).

---

## Change 3 — `configure.in`: `AC_DEFINE` block

**File:** `configure.in`
**Anchor:** after line 1438 (the closing `fi` of `if test "$WITH_OSGEARTH" = "yes"; then AC_DEFINE([WITH_OSGEARTH]…)`), before line 1440 (`if test "$WITH_AKAROA" …`)

**Surrounding context (from the real file):**
```
1430: # note: defines for OSG and osgEarth must be available even if WITH_QTENV=no
1431: 
1432: if test "$WITH_OSG" = "yes"; then
1433:   AC_DEFINE([WITH_OSG], [], [])
1434: fi
1435: 
1436: if test "$WITH_OSGEARTH" = "yes"; then
1437:   AC_DEFINE([WITH_OSGEARTH], [], [])
1438: fi
1439: 
1440: if test "$WITH_AKAROA" = "yes"; then
```

**Insert after line 1438 (after the `fi` closing the `WITH_OSGEARTH` block):**

```sh
if test "$WITH_USD" = "yes"; then
  AC_DEFINE([WITH_USD], [], [])
fi

if test "$WITH_USD_GEO" = "yes"; then
  AC_DEFINE([WITH_USD_GEO], [], [])
fi
```

---

## Change 4 — `configure.in`: `AC_SUBST` groups

### 4a — library flag variables

**Anchor:** after line 1555 (`AC_SUBST(OSGEARTH_LIBS)`), before line 1556 (`AC_SUBST(ZLIB_CFLAGS)`)

**Surrounding context (from the real file):**
```
1552: AC_SUBST(OSG_CFLAGS)
1553: AC_SUBST(OSG_LIBS)
1554: AC_SUBST(OSGEARTH_CFLAGS)
1555: AC_SUBST(OSGEARTH_LIBS)
1556: AC_SUBST(ZLIB_CFLAGS)
```

**Insert after line 1555 (`AC_SUBST(OSGEARTH_LIBS)`):**

```sh
AC_SUBST(USD_CFLAGS)
AC_SUBST(USD_LIBS)
AC_SUBST(PROJ_CFLAGS)
AC_SUBST(PROJ_LIBS)
```

### 4b — WITH_* boolean variables

**Anchor:** after line 1576 (`AC_SUBST(WITH_OSGEARTH)`), before line 1577 (`AC_SUBST(WITH_AKAROA)`)

**Surrounding context (from the real file):**
```
1575: AC_SUBST(WITH_OSG)
1576: AC_SUBST(WITH_OSGEARTH)
1577: AC_SUBST(WITH_AKAROA)
```

**Insert after line 1576 (`AC_SUBST(WITH_OSGEARTH)`):**

```sh
AC_SUBST(WITH_USD)
AC_SUBST(WITH_USD_GEO)
```

---

## Change 5 — `include/omnetpp/platdep/config.h.in`

**File:** `include/omnetpp/platdep/config.h.in`
**Anchor:** after line 11 (`#undef WITH_OSGEARTH`), before line 12 (`#undef WITH_AKAROA`)

**Surrounding context (from the real file):**
```
 9: #undef WITH_QTENV
10: #undef WITH_OSG
11: #undef WITH_OSGEARTH
12: #undef WITH_AKAROA
```

**Insert after line 11 (`#undef WITH_OSGEARTH`):**

```c
#undef WITH_USD
#undef WITH_USD_GEO
```

---

## Summary of all insertion points

| # | File | Anchor (real line / text) | What is inserted |
|---|---|---|---|
| 1 | `configure.user` | after `WITH_OSGEARTH=no` (line 89) | `WITH_USD=no` + `WITH_USD_GEO=no` blocks |
| 2 | `configure.in` | after `fi # WITH_OSG` (line 1180) | Full USD/PROJ detection section |
| 3 | `configure.in` | after `AC_DEFINE([WITH_OSGEARTH]…)` / `fi` (line 1438) | `AC_DEFINE` for `WITH_USD` and `WITH_USD_GEO` |
| 4a | `configure.in` | after `AC_SUBST(OSGEARTH_LIBS)` (line 1555) | `AC_SUBST` for `USD_CFLAGS/LIBS`, `PROJ_CFLAGS/LIBS` |
| 4b | `configure.in` | after `AC_SUBST(WITH_OSGEARTH)` (line 1576) | `AC_SUBST` for `WITH_USD`, `WITH_USD_GEO` |
| 5 | `include/omnetpp/platdep/config.h.in` | after `#undef WITH_OSGEARTH` (line 11) | `#undef WITH_USD`, `#undef WITH_USD_GEO` |

---

## Properties confirmed

- **WITH_USD defaults to `no`**: a `WITH_USD=no` build is byte-identical to today — no
  new object files, no new `config.h` symbols.
- **C++17 already enforced**: `configure.in` lines 278–295 mandate C++17; `configure.user`
  line 63 sets `CXXFLAGS=-std=c++17`.  OpenUSD 25.11+ requires C++17.  No additional
  language-standard wiring is needed.
- **USD_CFLAGS/USD_LIBS user-overridable**: identical pattern to `OSG_CFLAGS`/`OSG_LIBS`.
- **D6 (HgiMetal on macOS)** and **Q9 (Windows=WSL2)** documented as comments inside the
  detection block (Change 2).
- **No existing OSG lines removed or altered.**
