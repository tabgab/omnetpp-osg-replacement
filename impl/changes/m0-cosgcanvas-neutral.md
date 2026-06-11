# Change Note: m0-cosgcanvas-neutral

**Milestone:** M0 — Neutral public API  
**Kind:** MODIFY (two existing OMNeT++ source files)  
**Dependency:** `m0-cenvir-refscenenode` (must land first — see §3)

---

## Summary

Retype the `cOsgCanvas` scene-root field and all related signatures from `osg::Node*` to
`cScene3DNode*`.  Under `WITH_OSG`, `cScene3DNode` is a type alias for `osg::Node`
(defined in the new `include/omnetpp/scene3dnode.h` introduced by `m0-scene3dnode`), so
every retyped signature is **token-for-token type-identical** after alias expansion: no
ABI or API change for any existing `WITH_OSG` consumer (INET's `OsgScene.cc`, the qtenv
OSG plugin's `osgviewer.cc`, `osg.msg`'s `osg::Node *scene` descriptor field, etc.).

Without `WITH_OSG`, `cScene3DNode` is a forward-declared opaque class (`class cScene3DNode;`
in `scene3dnode.h`) — the kernel never dereferences it, preserving the invariant of "no
linker reference to OSG libs."

---

## File 1 — `include/omnetpp/cosgcanvas.h`

**Full path:** `/Users/gabortabi/DEV/omnetpp-6.4.0aipre2/include/omnetpp/cosgcanvas.h`

### Edit 1.1 — Replace OSG forward declaration with `scene3dnode.h` include

**Anchor:** lines 20–23 (immediately after `#include "ccanvas.h"`)

```cpp
// OLD (lines 20–23):
#include "ccanvas.h"

// don't include OSG headers
namespace osg { class Node;  }
```

```cpp
// NEW:
#include "ccanvas.h"
#include "scene3dnode.h"
```

**Rationale:** `scene3dnode.h` provides `cScene3DNode` (as `osg::Node` alias under
`WITH_OSG`, or opaque forward-declaration otherwise), making the forward declaration of
`osg::Node` redundant.  Placing it directly after `ccanvas.h` follows the existing
include ordering convention in the file.

---

### Edit 1.2 — Retype the class doc comment (lines 27–41)

**Anchor:** lines 27–41 (class Doxygen block for `cOsgCanvas`)

```cpp
// OLD (lines 27–41):
/**
 * @brief Wraps an OpenSceneGraph scene, allowing 3D visualization in graphical
 * user interfaces that support it (currently Qtenv).
 *
 * This class only wraps an OSG scene (as an osg::Node* pointer) and some
 * visualization hints, other tasks like setting up a 3D viewer window are
 * taken care of by the OMNeT++ user interface code (Qtenv). The scene graph
 * can be assembled using the OSG API, e.g. using osgDB::readNodeFile() or
 * creating and adding nodes directly.
 *
 * Since OpenSceneGraph is not a mandatory part of OMNeT++, it is recommended
 * that you surround your OSG code with \#ifdef WITH_OSG.
 *
 * @ingroup OSG
 */
```

```cpp
// NEW:
/**
 * @brief Wraps a 3D scene, allowing visualization in graphical user interfaces
 * that support it (currently Qtenv).
 *
 * The scene root is a renderer-neutral @c cScene3DNode* handle. Under @c WITH_OSG,
 * @c cScene3DNode is a type alias for @c osg::Node, so all existing OSG call sites
 * (e.g. @c osgCanvas->setScene(topLevelScene)) compile unchanged. Without @c WITH_OSG
 * it is an opaque forward-declared type; the active environment manages its lifetime
 * via cEnvir::refSceneNode() / unrefSceneNode() (which map to @c osg::Node::ref() /
 * unref() under @c WITH_OSG, and to no-ops in headless Cmdenv).
 *
 * Visualization hints (viewer style, camera manipulator, field of view, etc.) are
 * renderer-independent and remain unchanged.
 *
 * Since OpenSceneGraph is not a mandatory part of OMNeT++, it is recommended
 * that you surround OSG-specific scene-graph construction with \#ifdef WITH_OSG.
 *
 * @ingroup OSG
 */
```

**Rationale:** The class now holds a renderer-neutral handle; the doc comment must
reflect this and explain the alias relationship so API readers understand compatibility.
The `@ingroup OSG` tag is kept (Decision D1/Q5: do not rename the class or its group).

---

### Edit 1.3 — Retype the `scene` field (line 133)

**Anchor:** line 133 inside `protected:`

```cpp
// OLD (line 133):
        osg::Node *scene;  // reference counted
```

```cpp
// NEW (line 133):
        cScene3DNode *scene;  // reference counted (under WITH_OSG; opaque otherwise)
```

**Rationale:** The field type must match `setScene`/`getScene` and the constructor
parameter after they are retyped.  Under `WITH_OSG` the alias makes this identical to
the old declaration.

---

### Edit 1.4 — Retype the constructor declaration (line 153)

**Anchor:** line 153 inside `/** @name Constructors, destructor, assignment */`

```cpp
// OLD (line 153):
        cOsgCanvas(const char *name=nullptr, ViewerStyle viewerStyle=STYLE_GENERIC, osg::Node *scene=nullptr);
```

```cpp
// NEW (line 153):
        cOsgCanvas(const char *name=nullptr, ViewerStyle viewerStyle=STYLE_GENERIC, cScene3DNode *scene=nullptr);
```

**Rationale:** Constructor parameter must match the retyped field.  Under `WITH_OSG` the
alias makes the mangled symbol identical, preserving binary compatibility.

---

### Edit 1.5 — Retype `setScene` declaration and update doc comment (lines 165–172)

**Anchor:** lines 165–172, the `/** @name OSG scene. */` group

```cpp
// OLD (lines 165–172):
        /** @name OSG scene. */
        //@{
        /**
         * Set the 3D scene to be displayed. Note that osg::Node implements
         * reference counting, and setScene() increments the reference count.
         */
        virtual void setScene(osg::Node *scene);
```

```cpp
// NEW:
        /** @name OSG scene. */
        //@{
        /**
         * Set the 3D scene to be displayed. Reference counting is delegated to
         * the active environment: cEnvir::refSceneNode() is called on the new
         * scene and cEnvir::unrefSceneNode() on the old one.  Under @c WITH_OSG
         * these forward to @c osg::Node::ref() / unref() in the dynamically
         * loaded viewer library, preserving the "no static linker reference to
         * OSG" invariant.
         */
        virtual void setScene(cScene3DNode *scene);
```

**Rationale:** Parameter retyped from `osg::Node*` to `cScene3DNode*`.  The doc comment
is updated to describe the new delegation path through `cEnvir::refSceneNode()` /
`unrefSceneNode()` rather than naming OSG directly, matching the implementation change in
`cosgcanvas.cc`.

---

### Edit 1.6 — Retype `getScene` definition (line 176)

**Anchor:** line 176 (inline getter in the `/** @name OSG scene. */` group)

```cpp
// OLD (line 176):
        virtual osg::Node *getScene() const {return scene;}
```

```cpp
// NEW (line 176):
        virtual cScene3DNode *getScene() const {return scene;}
```

**Rationale:** Return type must match the retyped `scene` field.  Under `WITH_OSG` the
alias makes this type-identical to the original.

---

## File 2 — `src/sim/cosgcanvas.cc`

**Full path:** `/Users/gabortabi/DEV/omnetpp-6.4.0aipre2/src/sim/cosgcanvas.cc`

### Edit 2.1 — Update the implementation note (lines 18–28)

**Anchor:** lines 18–28 (block comment beginning `Implementation note:`)

```cpp
// OLD (lines 18–28):
/*
 * Implementation note: We don't want OMNeT++ to link directly with the OSG libraries
 * even when when WITH_OSG is defined, because they pull in tons of other libraries
 * while most simulations don't use 3D visualization at all. OSG support is loaded
 * dynamically by Qtenv from a shared lib when needed.
 *
 * Therefore, cOsgCanvas MUST NOT CREATE ANY LINKER REFERENCE TO THE OSG LIBS.
 * osg::Node::ref() and unref(), the use of which cannot be avoided, are accessed
 * via cEnvir::refOsgNode() and unrefOsgNode() that delegate to the dynamically
 * loaded OSG support library.
 */
```

```cpp
// NEW:
/*
 * Implementation note: We don't want OMNeT++ to link directly with the OSG libraries
 * even when WITH_OSG is defined, because they pull in tons of other libraries while
 * most simulations don't use 3D visualization at all. OSG support is loaded
 * dynamically by Qtenv from a shared lib when needed.
 *
 * Therefore, cOsgCanvas MUST NOT CREATE ANY LINKER REFERENCE TO THE OSG LIBS.
 * Scene-node reference counting (osg::Node::ref()/unref() under WITH_OSG) is reached
 * through the renderer-neutral cEnvir::refSceneNode() / unrefSceneNode().  By default
 * those forward to the deprecated cEnvir::refOsgNode() / unrefOsgNode(), keeping the
 * dynamic-library indirection intact.  Headless environments (cNullEnvir, EnvirBase)
 * provide no-op overrides so constructing a cOsgCanvas without a viewer is safe.
 */
```

**Rationale:** The comment must reflect the new delegation path
(`refSceneNode`/`unrefSceneNode`) rather than the old method names, and must mention the
headless/no-op case explicitly.

---

### Edit 2.2 — Retype file-local `ref` / `unref` helpers (lines 42–52)

**Anchor:** lines 42–52 (two inline file-local helpers before the constructor)

```cpp
// OLD (lines 42–52):
inline void ref(osg::Node *scene)
{
    if (scene)
        getEnvir()->refOsgNode(scene);
}

inline void unref(osg::Node *scene)
{
    if (scene)
        getEnvir()->unrefOsgNode(scene);
}
```

```cpp
// NEW:
inline void ref(cScene3DNode *scene)
{
    if (scene)
        getEnvir()->refSceneNode(scene);
}

inline void unref(cScene3DNode *scene)
{
    if (scene)
        getEnvir()->unrefSceneNode(scene);
}
```

**Rationale:** Both parameter types and method names are updated.  `refOsgNode` /
`unrefOsgNode` are no longer called directly from this translation unit; they remain
available as deprecated forwarding targets inside `EnvirBase`.  Under `WITH_OSG` the
alias makes `cScene3DNode` identical to `osg::Node`, so the generated code is
unchanged.

---

### Edit 2.3 — Retype the constructor definition parameter (line 54)

**Anchor:** line 54 (constructor definition)

```cpp
// OLD (line 54):
cOsgCanvas::cOsgCanvas(const char *name, ViewerStyle viewerStyle, osg::Node *scene) : cOwnedObject(name),
```

```cpp
// NEW (line 54):
cOsgCanvas::cOsgCanvas(const char *name, ViewerStyle viewerStyle, cScene3DNode *scene) : cOwnedObject(name),
```

**Rationale:** Definition parameter must match the declaration (Edit 1.4).

---

### Edit 2.4 — Retype the `setScene` definition parameter (line 96)

**Anchor:** line 96 (setScene definition)

```cpp
// OLD (line 96):
void cOsgCanvas::setScene(osg::Node *scene)
```

```cpp
// NEW (line 96):
void cOsgCanvas::setScene(cScene3DNode *scene)
```

**Rationale:** Definition parameter must match the retyped declaration (Edit 1.5).

---

## 3. Dependency

This change note depends on **`m0-cenvir-refscenenode`** landing first.  That item adds
`cEnvir::refSceneNode(cScene3DNode*)` and `unrefSceneNode(cScene3DNode*)` (with default
forwarding to the deprecated `refOsgNode`/`unrefOsgNode`) and matching no-op overrides in
`cNullEnvir` and `EnvirBase`.  Without those methods the bodies in Edit 2.2 do not
compile.

---

## 4. Exhaustive `osg::Node` occurrence list in the two files

Verification against the real source files (read 2026-06-11):

### `include/omnetpp/cosgcanvas.h`

| Line | Original text | Replacement |
|------|--------------|-------------|
| 23 | `namespace osg { class Node;  }` | removed; replaced by `#include "scene3dnode.h"` (Edit 1.1) |
| 133 | `osg::Node *scene;  // reference counted` | `cScene3DNode *scene;  // reference counted (under WITH_OSG; opaque otherwise)` (Edit 1.3) |
| 153 | `cOsgCanvas(const char *name=nullptr, ViewerStyle viewerStyle=STYLE_GENERIC, osg::Node *scene=nullptr);` | `cScene3DNode *scene=nullptr` (Edit 1.4) |
| 171 | `virtual void setScene(osg::Node *scene);` | `virtual void setScene(cScene3DNode *scene);` (Edit 1.5) |
| 176 | `virtual osg::Node *getScene() const {return scene;}` | `virtual cScene3DNode *getScene() const {return scene;}` (Edit 1.6) |

All five `osg::Node` occurrences in the header are accounted for.  No hint structs,
enums, or their members are touched (Decision D1/Q5: names kept).

### `src/sim/cosgcanvas.cc`

| Lines | Original text | Replacement |
|-------|--------------|-------------|
| 42 | `inline void ref(osg::Node *scene)` | `inline void ref(cScene3DNode *scene)` (Edit 2.2) |
| 45 | `getEnvir()->refOsgNode(scene);` | `getEnvir()->refSceneNode(scene);` (Edit 2.2) |
| 48 | `inline void unref(osg::Node *scene)` | `inline void unref(cScene3DNode *scene)` (Edit 2.2) |
| 51 | `getEnvir()->unrefOsgNode(scene);` | `getEnvir()->unrefSceneNode(scene);` (Edit 2.2) |
| 54 | `cOsgCanvas::cOsgCanvas(const char *name, ViewerStyle viewerStyle, osg::Node *scene)` | `cScene3DNode *scene` (Edit 2.3) |
| 96 | `void cOsgCanvas::setScene(osg::Node *scene)` | `void cOsgCanvas::setScene(cScene3DNode *scene)` (Edit 2.4) |

All six `osg::Node` occurrences in the source file are accounted for.  After these edits,
`cosgcanvas.cc` contains no direct references to `refOsgNode` or `unrefOsgNode`.

---

## 5. Compatibility argument

### WITH_OSG builds (e.g. Qtenv + INET)

`scene3dnode.h` defines `using cScene3DNode = osg::Node;` under `WITH_OSG`.  After the
C++ preprocessor and name-lookup pass:

- `cScene3DNode *scene` ≡ `osg::Node *scene`
- `virtual void setScene(cScene3DNode *scene)` ≡ `virtual void setScene(osg::Node *scene)`
- `virtual cScene3DNode *getScene() const` ≡ `virtual osg::Node *getScene() const`

Every call site that passes or returns `osg::Node*` continues to compile without
modification.  In particular:

- INET's `OsgScene.cc`: `osgCanvas->setScene(topLevelScene)` — `topLevelScene` is
  `osg::Node*`, which is identical to `cScene3DNode*` via the alias.
- `osg.msg`'s descriptor `osg::Node *scene` — this descriptor lives in the OSG plugin
  (which is compiled `WITH_OSG`), so it also sees `cScene3DNode = osg::Node` and the
  field type resolves correctly.
- `osgviewer.cc`'s `static_cast<osg::Node*>(...)` / `osg::Node*` downcasts — identical
  under the alias.

The mangled symbol names in the shared library are determined by the underlying types
after alias resolution, so the `.so` ABI for `setScene`/`getScene`/constructor is
**unchanged**.

### Headless Cmdenv / cNullEnvir

`cNullEnvir` and `EnvirBase` provide no-op overrides of `refSceneNode`/`unrefSceneNode`
(supplied by `m0-cenvir-refscenenode`).  Constructing a `cOsgCanvas` with or without a
scene pointer in a headless simulation therefore works exactly as before — the `if
(scene)` guard in the `ref`/`unref` helpers (Edit 2.2) suppresses even the virtual
dispatch for null scenes.

### `osg.msg` descriptor field (`osg::Node *scene`)

The `osg::Node *scene` field in `osg.msg` (line 97) is compiled **inside the OSG
plugin** where `WITH_OSG` is always defined.  Under that compile environment, the
accessor it generates calls `cOsgCanvas::getScene()`, which returns `cScene3DNode*` ≡
`osg::Node*`.  No change to `osg.msg` is required.

---

## 6. What is NOT changed

- Class name `cOsgCanvas` — kept (Decision D1/Q5).
- Enums: `ViewerStyle`, `CameraManipulatorType` — kept.
- Structs: `Vec3d`, `Viewpoint`, `EarthViewpoint` — kept.
- All hint getters/setters — untouched.
- `osg.msg` — untouched (OSG-plugin internal; uses `osg::Node*` which remains valid
  under `WITH_OSG` via the alias).
