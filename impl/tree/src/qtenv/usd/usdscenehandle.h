//==========================================================================
//  USDSCENEHANDLE.H - part of
//
//                     OMNeT++/OMNEST
//            Discrete System Simulation in C++
//
//==========================================================================

/*--------------------------------------------------------------*
  Copyright (C) 1992-2017 Andras Varga
  Copyright (C) 2006-2017 OpenSim Ltd.

  This file is distributed WITHOUT ANY WARRANTY. See the file
  `license' for details on this and other legal matters.
*--------------------------------------------------------------*/

#ifndef __OMNETPP_QTENV_USD_USDSCENEHANDLE_H
#define __OMNETPP_QTENV_USD_USDSCENEHANDLE_H

// Completes the opaque omnetpp::cScene3DNode declared in <omnetpp/scene3dnode.h>
// for the OpenUSD backend. Only USD-aware code (the oppqtenv-usd plugin and
// simulation models that author USD scenes) may include this header; the
// simulation kernel itself never completes the type and stays free of any
// USD linker dependency (mirroring the old osgutil.h design for OSG).

#ifdef WITH_OSG
#error "usdscenehandle.h must not be included in WITH_OSG builds: there cScene3DNode aliases osg::Node."
#endif

#include <atomic>
#include <map>

#include <pxr/pxr.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/sdf/path.h>

#include <omnetpp/scene3dnode.h>

namespace omnetpp {

class cObject;

/**
 * @brief The concrete scene root handle for the OpenUSD 3D backend.
 *
 * Wraps a UsdStage plus a prim-path -> cObject* registry used for picking
 * (the replacement for cObjectOsgNode). Reference counting is intrusive and
 * driven through cEnvir::refSceneNode()/unrefSceneNode(), which the loaded
 * oppqtenv-usd plugin routes to ref()/unref() below.
 *
 * Typical model-side usage:
 * @code
 *   auto stage = PXR_NS::UsdStage::CreateInMemory();
 *   // ... author prims, Z-up, metersPerUnit=1 ...
 *   auto *handle = new cScene3DNode(stage);
 *   handle->bindObject(PXR_NS::SdfPath("/Network/host0"), hostModule);
 *   getOsgCanvas()->setScene(handle);   // canvas refs the handle
 * @endcode
 */
class cScene3DNode
{
  public:
    explicit cScene3DNode(PXR_NS::UsdStageRefPtr stage) : stage(stage) {}

    PXR_NS::UsdStageRefPtr getStage() const { return stage; }

    /** Associate a prim (subtree root) with a simulation object for picking. */
    void bindObject(const PXR_NS::SdfPath& path, cObject *obj) { objectRegistry[path] = obj; }

    /** Resolve a prim path to a bound simulation object, or nullptr. */
    cObject *lookupObject(const PXR_NS::SdfPath& path) const {
        auto it = objectRegistry.find(path);
        return it == objectRegistry.end() ? nullptr : it->second;
    }

    /** Walk ancestors of @p path until a bound object is found (NodePath-walk analogue). */
    cObject *lookupObjectOrAncestor(PXR_NS::SdfPath path) const {
        while (!path.IsEmpty() && path != PXR_NS::SdfPath::AbsoluteRootPath()) {
            if (cObject *o = lookupObject(path))
                return o;
            path = path.GetParentPath();
        }
        return nullptr;
    }

    // Intrusive ref counting, driven via cEnvir::refSceneNode()/unrefSceneNode().
    void ref() { ++refCount; }
    void unref() { if (--refCount == 0) delete this; }

  private:
    ~cScene3DNode() {}  // delete via unref() only

    PXR_NS::UsdStageRefPtr stage;
    std::map<PXR_NS::SdfPath, cObject*> objectRegistry;
    std::atomic<int> refCount { 0 };
};

}  // namespace omnetpp

#endif
