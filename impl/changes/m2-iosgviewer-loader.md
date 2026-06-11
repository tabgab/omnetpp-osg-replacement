# Change Note: m2-iosgviewer-loader

**Milestone:** M2  
**Kind:** MODIFY  
**Target file:** `src/qtenv/iosgviewer.cc`  
**Header change:** none (see Out-of-scope section)

---

## Summary

Three targeted edits to `src/qtenv/iosgviewer.cc` make the existing plugin-loader prefer the
new `oppqtenv-usd` plugin over the legacy `oppqtenv-osg` one when the build includes OpenUSD
support, generalize the fallback-viewer error message away from OSG-specific wording, and
update the global comment that describes the factory pointer. No interface changes are made;
the `IOsgViewer`/`IOsgViewerFactory` seam, the `DummyOsgViewer` class, and all call sites
remain untouched.

---

## Edit 1 — `IOsgViewer::ensureViewerFactory()` (lines 42-73): preference-ordered plugin loop

### Before (lines 42-73)

```cpp
void IOsgViewer::ensureViewerFactory()
{
    if (!osgViewerFactory) {
        osgViewerFactory = &dummyOsgFactory;

        std::cout << "Loading OSG Viewer library... " << std::endl;

        try {
            loadExtensionLibrary("oppqtenv-osg");
        }
        catch (cRuntimeError &e) {
            std::cout << "Failed: " << e.what() << std::endl;
        }

        std::cout << std::endl;

        /*
        // Not throwing an exception here, because that will take the simulation into an "ERROR"
        // state, and will not let it continue - even though it could - , but after rebuilding the
        // network, it will.
        // And not showing a dialog at all because then it could get annoying when the user
        // doesn't have/want/care about 3D at all, but the model (INET with the 3D visualizations
        // for example) creates a cOsgCanvas - which is possible now. And it won't be a dummy
        // implementation anymore. And the dummy viewer will show an error anyways.
        if (osgViewerFactory == &dummyOsgFactory)
            getQtenv()->confirm(Qtenv::WARNING,
                                "Could not load the OSG viewer library (qtenv-osg).\n"
                                "Continuing without the ability to view the scenes of cOsgCanvas objects.\n"
                                "(Is " OMNETPP_PRODUCT " compiled with OSG support enabled?)");
        */
    }
}
```

### After (lines 42-73)

```cpp
void IOsgViewer::ensureViewerFactory()
{
    if (!osgViewerFactory) {
        osgViewerFactory = &dummyOsgFactory;

        // Try the available 3D viewer implementation libraries in order of
        // preference: USD (Hydra) first when built, then the legacy OSG one
        // (transitional, until OSG removal). Each library registers itself by
        // overwriting osgViewerFactory from a global object's constructor.
        std::vector<const char *> libs;
#ifdef WITH_USD
        libs.push_back("oppqtenv-usd");
#endif
#ifdef WITH_OSG
        libs.push_back("oppqtenv-osg");
#endif

        for (const char *lib : libs) {
            std::cout << "Loading 3D viewer library '" << lib << "'... " << std::endl;
            try {
                loadExtensionLibrary(lib);
            }
            catch (cRuntimeError& e) {
                std::cout << "Failed: " << e.what() << std::endl;
            }
            if (osgViewerFactory != &dummyOsgFactory)
                break;  // a real factory registered itself
        }

        std::cout << std::endl;

        /*
        // Not throwing an exception here, because that will take the simulation into an "ERROR"
        // state, and will not let it continue - even though it could - , but after rebuilding the
        // network, it will.
        // And not showing a dialog at all because then it could get annoying when the user
        // doesn't have/want/care about 3D at all, but the model (INET with the 3D visualizations
        // for example) creates a cOsgCanvas - which is possible now. And it won't be a dummy
        // implementation anymore. And the dummy viewer will show an error anyways.
        if (osgViewerFactory == &dummyOsgFactory)
            getQtenv()->confirm(Qtenv::WARNING,
                                "Could not load the OSG viewer library (qtenv-osg).\n"
                                "Continuing without the ability to view the scenes of cOsgCanvas objects.\n"
                                "(Is " OMNETPP_PRODUCT " compiled with OSG support enabled?)");
        */
    }
}
```

### Required additional include (near top of file, after existing `#include <iostream>`)

```cpp
#include <vector>
```

### Rationale and notes

**`WITH_USD` / `WITH_OSG` visibility.** Both macros are available in `iosgviewer.cc` through
the include chain: `iosgviewer.h` → `qtenvdefs.h` → `omnetpp/platdep/platdefs.h` →
`platdep/config.h` (the generated autoconf/configure output header). No new `#include`
directive is needed beyond `<vector>`.

**Why `std::vector` instead of a C-array.** When *neither* `WITH_USD` nor `WITH_OSG` is
defined (unlikely but legal: a build with no 3D support at all), a zero-length `const char*`
array would be ill-formed in C++. `std::vector` is always well-formed. `fsutils.h` already
pulls in `<vector>` transitively (see `envir/fsutils.h` line 19), but the explicit
`#include <vector>` in `iosgviewer.cc` makes the dependency self-documenting.

**Behavioral equivalence on `WITH_OSG`-only builds.** When only `WITH_OSG` is set, `libs`
contains exactly one entry (`"oppqtenv-osg"`); the loop body executes once, matching the
original code's single `loadExtensionLibrary("oppqtenv-osg")` call. The `Failed:` message
shape is preserved identically. No behavioral regression.

**Decision D5 — loader prefers USD.** When `WITH_USD` is set (alone or alongside
`WITH_OSG`), `"oppqtenv-usd"` is the first element and the loop `break`s immediately if it
registers a real factory. If it fails (e.g. missing OpenUSD runtime `.so`/`.dylib`), the loop
continues to the `"oppqtenv-osg"` entry (if present) — providing an automatic fallback to
the legacy renderer during the transitional period.

**The commented-out `confirm()` block (lines 58-71) is preserved unchanged.** Its wording
still references OSG, which is intentional: the block is deliberately dead code (see the
original comment explaining why no exception/dialog is shown), and updating its content is
not necessary for correctness. It documents historic intent.

---

## Edit 2 — `DummyOsgViewer::paintEvent` (lines 113-128): generalize the error string

### Before (lines 125-127)

```cpp
    painter.drawText(rect.adjusted(50, 50, -50, -50), // so the toolbar won't ob
                     "The OSG Viewer library could not be loaded.\n"
                     "(Was " OMNETPP_PRODUCT " compiled with OSG support enabled?)");
```

### After (lines 125-127)

```cpp
    painter.drawText(rect.adjusted(50, 50, -50, -50), // so the toolbar won't ob
                     "The 3D viewer library could not be loaded.\n"
                     "(Was " OMNETPP_PRODUCT " compiled with 3D support (USD or OSG) enabled?)");
```

### Rationale

`DummyOsgViewer` is the shared fallback for *all* 3D viewer implementations (USD and OSG).
The original message tells the user the "OSG viewer library" could not be loaded even when the
build is a `WITH_USD`-only configuration that never attempted to load the OSG plugin. The
generalised wording correctly describes the actual situation regardless of which plugin was
attempted.

---

## Edit 3 — Line-27 comment: generalize the factory-pointer description

### Before (line 27)

```cpp
// the osg implementation library will set this pointer to a real factory in it
QTENV_API IOsgViewerFactory *osgViewerFactory = nullptr;
```

### After (line 27)

```cpp
// the 3D viewer implementation library (oppqtenv-usd or oppqtenv-osg) sets this pointer to its real factory when loaded
QTENV_API IOsgViewerFactory *osgViewerFactory = nullptr;
```

### Rationale

The comment named only the OSG plugin. Once `oppqtenv-usd` is introduced it is equally valid
(and the preferred) writer of this pointer. The updated wording documents both plugins and the
mechanism (global-object constructor self-registration on dynamic load).

---

## Out of scope — explicit rationale

### No header changes (`iosgviewer.h`)

The `IOsgViewer` and `IOsgViewerFactory` interfaces are intentionally left unchanged
(Decision Q5 in the architecture). The seam was designed to be renderer-agnostic: neither
interface type nor any method signature refers to OSG directly. `UsdViewerFactory` (the
`oppqtenv-usd` plugin's implementation) will simply implement `IOsgViewerFactory` as-is.
Renaming the interfaces would force a flag-day change across every call site (`inspectorfactory.cc`,
`moduleinspector.cc`, Qtenv startup, the OSG plugin's own registration) for zero runtime
benefit during the M2 transitional phase.

### `isOsgPreferred()` needs no edit (lines 108-111)

```cpp
bool IOsgViewer::isOsgPreferred()
{
    return osgViewerFactory != nullptr && osgViewerFactory != &dummyOsgFactory;
}
```

The implementation is already renderer-agnostic: it returns `true` if and only if *a real
factory* is registered — i.e. any factory other than `dummyOsgFactory`. When `UsdViewerFactory`
registers itself, `osgViewerFactory` points to it (not to `dummyOsgFactory`), so the function
returns `true` and the module inspector correctly defaults to 3D mode. No change is needed.
The name `isOsgPreferred` is misleading in a USD-first build, but renaming it is a cosmetic
concern deferred beyond M2.

### `Qt::AA_ShareOpenGLContexts` is an M3 change

Setting `QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts)` before `QApplication`
is created is required when multiple `UsdViewer` instances share a single
`Hgi`/`HdDriver` across inspectors. This must be added in the Qtenv startup path (`qtenv.cc`
or `main.cc`), not here. That change is scoped to M3.

---

## Unified diff

```diff
--- a/src/qtenv/iosgviewer.cc
+++ b/src/qtenv/iosgviewer.cc
@@ -17,6 +17,7 @@
 #include <iostream>
+#include <vector>
 #include "iosgviewer.h"
 #include "envir/fsutils.h"
 #include "common/ver.h"
@@ -27,7 +28,7 @@
 namespace qtenv {
 
-// the osg implementation library will set this pointer to a real factory in it
+// the 3D viewer implementation library (oppqtenv-usd or oppqtenv-osg) sets this pointer to its real factory when loaded
 QTENV_API IOsgViewerFactory *osgViewerFactory = nullptr;
 
@@ -42,19 +43,27 @@ void IOsgViewer::ensureViewerFactory()
 {
     if (!osgViewerFactory) {
         osgViewerFactory = &dummyOsgFactory;
 
-        std::cout << "Loading OSG Viewer library... " << std::endl;
-
-        try {
-            loadExtensionLibrary("oppqtenv-osg");
-        }
-        catch (cRuntimeError &e) {
-            std::cout << "Failed: " << e.what() << std::endl;
+        // Try the available 3D viewer implementation libraries in order of
+        // preference: USD (Hydra) first when built, then the legacy OSG one
+        // (transitional, until OSG removal). Each library registers itself by
+        // overwriting osgViewerFactory from a global object's constructor.
+        std::vector<const char *> libs;
+#ifdef WITH_USD
+        libs.push_back("oppqtenv-usd");
+#endif
+#ifdef WITH_OSG
+        libs.push_back("oppqtenv-osg");
+#endif
+
+        for (const char *lib : libs) {
+            std::cout << "Loading 3D viewer library '" << lib << "'... " << std::endl;
+            try {
+                loadExtensionLibrary(lib);
+            }
+            catch (cRuntimeError& e) {
+                std::cout << "Failed: " << e.what() << std::endl;
+            }
+            if (osgViewerFactory != &dummyOsgFactory)
+                break;  // a real factory registered itself
         }
 
         std::cout << std::endl;
@@ -124,7 +133,7 @@ void DummyOsgViewer::paintEvent(QPaintEvent *)
 
     painter.setPen(QPen(pal.text().color()));
     painter.drawText(rect.adjusted(50, 50, -50, -50), // so the toolbar won't ob
-                     "The OSG Viewer library could not be loaded.\n"
-                     "(Was " OMNETPP_PRODUCT " compiled with OSG support enabled?)");
+                     "The 3D viewer library could not be loaded.\n"
+                     "(Was " OMNETPP_PRODUCT " compiled with 3D support (USD or OSG) enabled?)");
 }
```
