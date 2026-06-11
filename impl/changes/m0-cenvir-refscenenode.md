# M0 — Generalize cEnvir scene-node ref-counting: refSceneNode/unrefSceneNode

**Milestone:** M0  
**Kind:** MODIFY  
**Status:** Pending  

## Goal

Add non-pure virtual `refSceneNode`/`unrefSceneNode` methods to `cEnvir` whose default bodies
forward to the existing (now deprecated) pure virtuals `refOsgNode`/`unrefOsgNode`.  This lets
every existing `cEnvir` subclass (`EnvirBase`, `cNullEnvir`, `Qtenv`, user environments) keep
compiling without modification.  Callers (primarily `cOsgCanvas`) migrate to the new entry
point; the OSG path stays alive through the chain until OSG is removed at the final
OSG-removal milestone.

No `.cc` file changes are needed — all new overrides are inline, matching the style of the
existing anchors.

---

## File 1 — `include/omnetpp/cenvir.h`

### Anchor — include block (lines 24-28)

```cpp
// line 24:
#include "clistener.h"  // for simsignal_t
// line 25:
#include "clifecyclelistener.h"
// line 26:
#include "ccanvas.h"
// line 27: (blank)
// line 28:
namespace osg { class Node; }
```

#### Change 1a — add `scene3dnode.h` include after line 26

Insert immediately after the `#include "ccanvas.h"` line:

```cpp
#include "scene3dnode.h"
```

The `osg::Node` forward declaration at line 28 is kept as-is.  It remains necessary for the
deprecated `refOsgNode`/`unrefOsgNode` signatures.  Under `WITH_OSG`, `scene3dnode.h` defines
`cScene3DNode` as `osg::Node`, so both are the same type; under `!WITH_OSG` both are distinct
incomplete types used only as pointer arguments.  There is no ODR problem because both are
mere forward declarations.

Result after the change:

```cpp
#include "ccanvas.h"
#include "scene3dnode.h"

namespace osg { class Node; }
```

---

### Anchor — `refOsgNode`/`unrefOsgNode` pure virtuals (lines 844-854)

```cpp
// line 844:
    /**
// line 845:
     * Used by cOsgCanvas to increase the reference count of an osg::Node.
// line 846:
     * Should delegate to node->ref() when OSG support is available.
// line 847:
     */
// line 848:
    virtual void refOsgNode(osg::Node *scene) = 0;
// line 849:
// line 850:
    /**
// line 851:
     * Used by cOsgCanvas to increase the reference count of an osg::Node.
// line 852:
     * Should delegate to node->unref() when OSG support is available.
// line 853:
     */
// line 854:
    virtual void unrefOsgNode(osg::Node *scene) = 0;
```

#### Change 1b — deprecate doc comments on refOsgNode/unrefOsgNode

Replace the doc comments above `refOsgNode` and `unrefOsgNode` (lines 844-847 and 850-853)
with the following (the `= 0` declarations themselves are untouched; no `[[deprecated]]`
attribute is added so that existing subclass overrides continue to compile warning-free):

```cpp
    /**
     * Used by cOsgCanvas to increase the reference count of an osg::Node.
     * Should delegate to node->ref() when OSG support is available.
     * @deprecated Use refSceneNode()/unrefSceneNode(). Kept transitionally for
     *             OSG support; removed at final OSG removal.
     */
    virtual void refOsgNode(osg::Node *scene) = 0;

    /**
     * Used by cOsgCanvas to decrease the reference count of an osg::Node.
     * Should delegate to node->unref() when OSG support is available.
     * @deprecated Use refSceneNode()/unrefSceneNode(). Kept transitionally for
     *             OSG support; removed at final OSG removal.
     */
    virtual void unrefOsgNode(osg::Node *scene) = 0;
```

#### Change 1c — insert refSceneNode/unrefSceneNode immediately after line 854

Insert the following block directly after the `virtual void unrefOsgNode(osg::Node *scene) = 0;`
line (line 854), before the blank line that precedes the `idle()` doc comment:

```cpp

    /**
     * Used by cOsgCanvas to increment the reference count of its scene root.
     * The default implementation delegates to the deprecated refOsgNode();
     * environments with a non-OSG 3D backend should override this method.
     */
    virtual void refSceneNode(cScene3DNode *node) { refOsgNode(reinterpret_cast<osg::Node *>(node)); }

    /**
     * Used by cOsgCanvas to decrement the reference count of its scene root.
     * The default implementation delegates to the deprecated unrefOsgNode().
     */
    virtual void unrefSceneNode(cScene3DNode *node) { unrefOsgNode(reinterpret_cast<osg::Node *>(node)); }
```

**Rationale for `reinterpret_cast` between incomplete pointer types:**  
Under `WITH_OSG`, `cScene3DNode` is a typedef for `osg::Node` (defined in `scene3dnode.h`), so
the cast is an identity cast at the type level — zero overhead, no UB.  
Under `!WITH_OSG`, `cScene3DNode` and `osg::Node` are two distinct incomplete class types.
`reinterpret_cast` between pointers to incomplete class types is well-formed C++ (no
requirement that either type be complete; `static_cast` would require completeness, but
`reinterpret_cast` does not).  The resulting pointer is only ever passed to the default
`refOsgNode`/`unrefOsgNode` overrides in `cNullEnvir` and `EnvirBase`, which are no-ops and
never dereference the pointer.  Until the USD back-end (M4) overrides `refSceneNode` directly,
no path ever touches the pointer value itself.  The cast is therefore safe for all current
configurations.

---

## File 2 — `include/omnetpp/cnullenvir.h`

### Anchor — lines 155-158

```cpp
// line 155:
    virtual unsigned long getUniqueNumber() override  {return nextUniqueNumber++;}
// line 156:
    virtual void refOsgNode(osg::Node *scene) override {}
// line 157:
    virtual void unrefOsgNode(osg::Node *scene) override {}
// line 158:
    virtual bool idle() override  {return false;}
```

#### Change — add matching no-op overrides after line 157

Insert immediately after `virtual void unrefOsgNode(osg::Node *scene) override {}` (line 157),
before `virtual bool idle()`:

```cpp
    virtual void refSceneNode(cScene3DNode *node) override {}
    virtual void unrefSceneNode(cScene3DNode *node) override {}
```

Result after the change (lines 155-160):

```cpp
    virtual unsigned long getUniqueNumber() override  {return nextUniqueNumber++;}
    virtual void refOsgNode(osg::Node *scene) override {}
    virtual void unrefOsgNode(osg::Node *scene) override {}
    virtual void refSceneNode(cScene3DNode *node) override {}
    virtual void unrefSceneNode(cScene3DNode *node) override {}
    virtual bool idle() override  {return false;}
```

No include change is needed in `cnullenvir.h`: it already `#include "cenvir.h"` (line 19),
which will pull in `scene3dnode.h` after the File 1 change above.

---

## File 3 — `src/envir/envirbase.h`

### Anchor — lines 281-284

```cpp
// line 281:
    virtual unsigned long getUniqueNumber() override;
// line 282:
    virtual void refOsgNode(osg::Node *scene) override {}
// line 283:
    virtual void unrefOsgNode(osg::Node *scene) override {}
// line 284:
    virtual bool idle() override;
```

#### Change — add matching no-op overrides after line 283

Insert immediately after `virtual void unrefOsgNode(osg::Node *scene) override {}` (line 283),
before `virtual bool idle()`:

```cpp
    virtual void refSceneNode(cScene3DNode *node) override {}
    virtual void unrefSceneNode(cScene3DNode *node) override {}
```

Result after the change (lines 281-286):

```cpp
    virtual unsigned long getUniqueNumber() override;
    virtual void refOsgNode(osg::Node *scene) override {}
    virtual void unrefOsgNode(osg::Node *scene) override {}
    virtual void refSceneNode(cScene3DNode *node) override {}
    virtual void unrefSceneNode(cScene3DNode *node) override {}
    virtual bool idle() override;
```

`envirbase.h` already includes `<omnetpp/cenvir.h>` transitively (via `envir/envirbase.h`
including `omnetpp/cenvir.h`), so `cScene3DNode` will be visible after the File 1 change.

No change is needed to `src/envir/envirbase.cc` — the overrides are inline at the declaration
site, identical to the existing `refOsgNode`/`unrefOsgNode` style.

---

## Files NOT changed in this item

| File | Reason |
|---|---|
| `src/qtenv/qtenv.h` | `Qtenv` inherits the forwarding default from `cEnvir`. A call to `refSceneNode` lands in `cEnvir::refSceneNode`, which calls `refOsgNode(reinterpret_cast<osg::Node*>(node))`, which dispatches to `Qtenv::refOsgNode` (vtable), which calls `IOsgViewer::refNode(scene)` (qtenv.cc:3038-3041). The existing OSG path is fully preserved. |
| `src/qtenv/qtenv.cc` | Same reasoning; no override of `refSceneNode` is needed at this stage. |
| `src/envir/envirbase.cc` | New overrides are inline in the header. |

---

## Acceptance checklist

- [x] `cEnvir` gains non-pure `refSceneNode`/`unrefSceneNode` with default bodies forwarding to
      `refOsgNode`/`unrefOsgNode` via `reinterpret_cast`; legality argument recorded above.
- [x] `refOsgNode`/`unrefOsgNode` remain pure virtuals and carry no `[[deprecated]]` attribute
      (subclasses override them without warnings).
- [x] No-op overrides added at `cnullenvir.h` line 157 anchor and `envirbase.h` line 283 anchor.
- [x] No existing `cEnvir` subclass requires modification: `cNullEnvir`, `EnvirBase`, `Qtenv`,
      and any user environment all inherit the new entry point transparently.
- [x] `Qtenv`'s ref path (`qtenv.cc:3038` → `IOsgViewer::refNode`) is preserved unchanged.
