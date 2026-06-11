# M1 Change Note: `Makefile.inc.in` and `src/qtenv/Makefile`

**Item ID:** m1-makefileinc-qtenv  
**Milestone:** M1 — WITH_USD build plumbing  
**Kind:** MODIFY (change note only; no direct edits to the OMNeT++ source tree)  
**Target real files:**
- `/Users/gabortabi/DEV/omnetpp-6.4.0aipre2/Makefile.inc.in`
- `/Users/gabortabi/DEV/omnetpp-6.4.0aipre2/src/qtenv/Makefile`

---

## Validation argument (WITH_USD=no default)

With `WITH_USD=no` (the default when `configure` does not detect OpenUSD):

- The two new feature-flag assignments (`WITH_USD = @WITH_USD@` and
  `WITH_USD_GEO = @WITH_USD_GEO@`) expand to `WITH_USD = no` and `WITH_USD_GEO = no`.
- The four new compiler/linker variable assignments (`USD_CFLAGS`, `USD_LIBS`,
  `PROJ_CFLAGS`, `PROJ_LIBS`) expand to empty strings — they are never referenced and
  therefore have no effect on any recipe.
- Every new `ifeq ($(WITH_USD),yes)` block collapses to a no-op.
- The new `.PHONY` entry `qtenv-usd` is registered but the `qtenv-usd` target never exists
  (its defining block is inside the `ifeq` that collapses), so nothing changes.
- The new `$(Q)$(MAKE) clean -C usd` line in the `clean` target runs unconditionally
  (mirrors the OSG pattern); it is safe only when `src/qtenv/usd/` exists with its own
  `Makefile` (delivered by item `m1-usd-plugin-makefile`). When `WITH_USD=no` *and* the
  `usd/` directory is present, `make clean -C usd` cleans any stale build artifacts without
  affecting the overall build.

The generated `Makefile.inc` is therefore **behaviorally identical** to today's except for
four extra (unused) variable assignments.

---

## File 1: `Makefile.inc.in`

### Insertion 1 — Feature flags (after line 20)

**Anchor — existing lines 19–21 (quote exact text):**

```makefile
WITH_OSG = @WITH_OSG@
WITH_OSGEARTH = @WITH_OSGEARTH@
WITH_NETBUILDER = @WITH_NETBUILDER@
```

**Insert after line 20** (`WITH_OSGEARTH = @WITH_OSGEARTH@`):

```makefile
WITH_USD = @WITH_USD@
WITH_USD_GEO = @WITH_USD_GEO@
```

**Result — lines 19–23 after the edit:**

```makefile
WITH_OSG = @WITH_OSG@
WITH_OSGEARTH = @WITH_OSGEARTH@
WITH_USD = @WITH_USD@
WITH_USD_GEO = @WITH_USD_GEO@
WITH_NETBUILDER = @WITH_NETBUILDER@
```

---

### Insertion 2 — Compiler/linker flag variables (after line 202)

**Anchor — existing lines 199–204 (quote exact text):**

```makefile
OSG_CFLAGS = @OSG_CFLAGS@
OSG_LIBS = @OSG_LIBS@
OSGEARTH_CFLAGS = @OSGEARTH_CFLAGS@
OSGEARTH_LIBS = @OSGEARTH_LIBS@
ZLIB_CFLAGS = @ZLIB_CFLAGS@
ZLIB_LIBS = @ZLIB_LIBS@
```

**Insert after line 202** (`OSGEARTH_LIBS = @OSGEARTH_LIBS@`):

```makefile
USD_CFLAGS = @USD_CFLAGS@
USD_LIBS = @USD_LIBS@
PROJ_CFLAGS = @PROJ_CFLAGS@
PROJ_LIBS = @PROJ_LIBS@
```

**Result — lines 199–210 after the edit:**

```makefile
OSG_CFLAGS = @OSG_CFLAGS@
OSG_LIBS = @OSG_LIBS@
OSGEARTH_CFLAGS = @OSGEARTH_CFLAGS@
OSGEARTH_LIBS = @OSGEARTH_LIBS@
USD_CFLAGS = @USD_CFLAGS@
USD_LIBS = @USD_LIBS@
PROJ_CFLAGS = @PROJ_CFLAGS@
PROJ_LIBS = @PROJ_LIBS@
ZLIB_CFLAGS = @ZLIB_CFLAGS@
ZLIB_LIBS = @ZLIB_LIBS@
OPENMP_FLAGS = @OPENMP_CXXFLAGS@
MPI_CFLAGS = @MPI_CFLAGS@
```

---

### Insertion 3 — Static-link block inside `WITH_QTENV` (after the `WITH_OSGEARTH` ifeq)

**Anchor — existing lines 241–258 (the full WITH_QTENV static-linking block):**

```makefile
ifeq ($(WITH_QTENV),yes)
  # libraries required for static linking
  ifneq ($(SHARED_LIBS),yes)
    QTENV_LIBS += $(QT_LIBS)
    ifeq ($(WITH_OSG),yes)
      QTENV_LIBS += $(OSG_LIBS)
      KERNEL_LIBS += -losg -lOpenThreads
    endif
    ifeq ($(WITH_OSGEARTH),yes)
      QTENV_LIBS += $(OSGEARTH_LIBS)
      KERNEL_LIBS += -losgEarth
    endif
    ifeq ($(PLATFORM),macos)
      QTENV_LIBS += -framework Carbon
    endif
  endif
  ALL_ENV_LIBS += $(QTENV_LIBS)
endif
```

**Insert after line 252** (the closing `endif` of the `WITH_OSGEARTH` block, i.e. after
`      KERNEL_LIBS += -losgEarth` / `    endif`):

```makefile
    ifeq ($(WITH_USD),yes)
      QTENV_LIBS += $(USD_LIBS)
    endif
```

**Result — lines 241–261 after the edit:**

```makefile
ifeq ($(WITH_QTENV),yes)
  # libraries required for static linking
  ifneq ($(SHARED_LIBS),yes)
    QTENV_LIBS += $(QT_LIBS)
    ifeq ($(WITH_OSG),yes)
      QTENV_LIBS += $(OSG_LIBS)
      KERNEL_LIBS += -losg -lOpenThreads
    endif
    ifeq ($(WITH_OSGEARTH),yes)
      QTENV_LIBS += $(OSGEARTH_LIBS)
      KERNEL_LIBS += -losgEarth
    endif
    ifeq ($(WITH_USD),yes)
      QTENV_LIBS += $(USD_LIBS)
    endif
    ifeq ($(PLATFORM),macos)
      QTENV_LIBS += -framework Carbon
    endif
  endif
  ALL_ENV_LIBS += $(QTENV_LIBS)
endif
```

**Rationale — why `KERNEL_LIBS` is NOT extended here:**

OSG adds `-losg -lOpenThreads` to `KERNEL_LIBS` (line 247) because the statically-linked
kernel (`liboppsim`) contains `cosgcanvas.cc`, which calls `osg::Referenced::ref()` and
`unref()` on `osg::Node` objects directly. When the kernel is statically linked into a
simulation, the linker must resolve those OSG symbols at simulation-link time; hence they
must appear in `KERNEL_LIBS`.

By contrast, the USD plugin (`oppqtenv-usd`) is a **dynamically-loaded extension library**.
`cosgcanvas.cc` reaches the USD viewer exclusively through `cEnvir` virtual dispatch
(`cEnvir::refSceneNode` / `unrefSceneNode` — introduced in M0), not through any direct USD
symbol. The OMNeT++ kernel therefore has **zero linker dependency on USD**. Adding USD libs
to `KERNEL_LIBS` would be wrong: it would force every USD-enabled simulation binary to link
against all of OpenUSD even when the simulation never uses `cOsgCanvas`, and it would break
static builds that do not have OpenUSD installed. `QTENV_LIBS` is sufficient: it covers the
case where Qtenv itself is statically linked (and thus the plugin cannot be a separate shared
library — in that configuration all plugin code is pulled into the same binary).

---

### Modification 4 — Help text variable list (line 370)

**Anchor — existing line 370 (exact text):**

```makefile
    WITH_QTENV, WITH_OSG, WITH_OSGEARTH, WITH_NETBUILDER, WITH_LIBXML, WITH_PARSIM, WITH_SYSTEMC
```

**Replace with:**

```makefile
    WITH_QTENV, WITH_OSG, WITH_OSGEARTH, WITH_USD, WITH_NETBUILDER, WITH_LIBXML, WITH_PARSIM, WITH_SYSTEMC
```

The insertion point is immediately after `WITH_OSGEARTH` and before `WITH_NETBUILDER`,
matching the order of the feature-flag declarations at the top of the file.

---

## File 2: `src/qtenv/Makefile`

### Modification 5 — `.PHONY` line (line 86)

**Anchor — existing line 86 (exact text):**

```makefile
.PHONY: all clean qtenv-osg
```

**Replace with:**

```makefile
.PHONY: all clean qtenv-osg qtenv-usd
```

---

### Insertion 6 — `qtenv-usd` build block (after the `qtenv-osg` block, after line 124)

**Anchor — existing lines 120–125 (quote exact text; NOTE: recipe line uses a TAB):**

```makefile
ifeq ($(WITH_OSG),yes)
all: qtenv-osg
qtenv-osg: $(TARGET_LIB_FILES)
	$(Q)$(MAKE) -C osg
endif
```

> TAB character precedes `$(Q)$(MAKE) -C osg` — this is a make recipe line.

**Insert after line 124** (the `endif` closing the `WITH_OSG` block) — the new block must
also use a TAB before its recipe line:

```makefile
ifeq ($(WITH_USD),yes)
all: qtenv-usd
qtenv-usd: $(TARGET_LIB_FILES)
	$(Q)$(MAKE) -C usd
endif
```

> TAB character precedes `$(Q)$(MAKE) -C usd`.

**Notes:**

- The new block is a **character-for-character mirror** of the OSG block with `osg` → `usd`
  and `WITH_OSG` → `WITH_USD`.
- Both blocks may be active simultaneously during the transition period (M1–M11). The build
  system enables whichever plugins are configured; the runtime loader preference (item
  `m2-iosgviewer-loader`) decides which one is actually used when both are present. There is
  no make-level conflict: `all` can depend on both `qtenv-osg` and `qtenv-usd`, and each
  sub-make is independent.
- `qtenv-usd` has a prerequisite of `$(TARGET_LIB_FILES)` — the same dependency as
  `qtenv-osg` — ensuring `liboppqtenv` is fully built and copied to `$(OMNETPP_LIB_DIR)`
  before the plugin sub-make runs (the plugin links against it via `-loppqtenv$D`).

**Result — lines 120–130 after the edit:**

```makefile
ifeq ($(WITH_OSG),yes)
all: qtenv-osg
qtenv-osg: $(TARGET_LIB_FILES)
	$(Q)$(MAKE) -C osg
endif

ifeq ($(WITH_USD),yes)
all: qtenv-usd
qtenv-usd: $(TARGET_LIB_FILES)
	$(Q)$(MAKE) -C usd
endif
```

---

### Insertion 7 — `clean` target: add USD sub-make (after line 165)

**Anchor — existing lines 162–166 (quote exact text; NOTE: recipe lines use TABs):**

```makefile
clean:
	$(qecho) Cleaning qtenv
	$(Q)rm -rf $O $(GENERATED_SOURCES) $(TARGET_LIB_FILES) $(DARK_SVGICONS) icons_dark.qrc
	$(Q)$(MAKE) clean -C osg
```

> TAB characters precede each recipe line.

**Insert after line 165** (`$(Q)$(MAKE) clean -C osg`):

```makefile
	$(Q)$(MAKE) clean -C usd
```

> TAB character precedes `$(Q)$(MAKE) clean -C usd`.

**Result — lines 162–167 after the edit:**

```makefile
clean:
	$(qecho) Cleaning qtenv
	$(Q)rm -rf $O $(GENERATED_SOURCES) $(TARGET_LIB_FILES) $(DARK_SVGICONS) icons_dark.qrc
	$(Q)$(MAKE) clean -C osg
	$(Q)$(MAKE) clean -C usd
```

**Rationale:**

The existing `$(Q)$(MAKE) clean -C osg` runs **unconditionally** regardless of `WITH_OSG`.
This mirrors the OMNeT++ convention: `clean` is total — it cleans every sub-directory that
could have been built, so that repeated `make clean` invocations remain idempotent even if
the user has toggled feature flags between builds.

The new `$(Q)$(MAKE) clean -C usd` must therefore also run unconditionally, for the same
reason. This is safe as long as `src/qtenv/usd/` exists as a checked-in directory containing
its own `Makefile` with a `clean` target (delivered by item `m1-usd-plugin-makefile`). When
`WITH_USD=no` and `usd/` has never been built, `make clean -C usd` simply finds nothing to
delete and exits 0.

---

## Summary table

| # | File | Anchor (line in original) | Action | Make syntax note |
|---|------|--------------------------|--------|-----------------|
| 1 | `Makefile.inc.in` | After line 20 (`WITH_OSGEARTH = …`) | Insert 2 lines: `WITH_USD`, `WITH_USD_GEO` | `@…@` substitution |
| 2 | `Makefile.inc.in` | After line 202 (`OSGEARTH_LIBS = …`) | Insert 4 lines: `USD_CFLAGS`, `USD_LIBS`, `PROJ_CFLAGS`, `PROJ_LIBS` | `@…@` substitution |
| 3 | `Makefile.inc.in` | After line 252 (close of `WITH_OSGEARTH` ifeq) | Insert USD `QTENV_LIBS` ifeq block (3 lines); **no** `KERNEL_LIBS` change | Indented with spaces (inside outer `ifneq`) |
| 4 | `Makefile.inc.in` | Line 370 (help text) | Insert `WITH_USD` after `WITH_OSGEARTH` in the variable list | Plain text in `define` block |
| 5 | `src/qtenv/Makefile` | Line 86 (`.PHONY`) | Append `qtenv-usd` | `.PHONY` directive |
| 6 | `src/qtenv/Makefile` | After line 124 (close of `WITH_OSG` block) | Insert `qtenv-usd` build block (5 lines) | Recipe line has **TAB** |
| 7 | `src/qtenv/Makefile` | Line 165 (after `clean -C osg`) | Insert `$(Q)$(MAKE) clean -C usd` | Recipe line has **TAB** |
