//==========================================================================
//  STAGECACHE.H - part of
//
//                     OMNeT++/OMNEST
//            Discrete System Simulation in C++
//
//==========================================================================

/*--------------------------------------------------------------*
  Copyright (C) 1992-2024 Andras Varga
  Copyright (C) 2006-2024 OpenSim Ltd.

  This file is distributed WITHOUT ANY WARRANTY. See the file
  `license' for details on this and other legal matters.
*--------------------------------------------------------------*/

#ifndef __OMNETPP_QTENV_USD_STAGECACHE_H
#define __OMNETPP_QTENV_USD_STAGECACHE_H

// StageCache is part of the oppqtenv-usd plugin which only builds when
// OpenUSD is present; pxr headers are therefore always available here.
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/tokens.h>

#include "omnetpp/scene3dnode.h"   // cScene3DNode (= osg::Node under WITH_OSG, opaque class otherwise)
#include "omnetpp/cobject.h"

#include <map>

// Explicit pxr:: qualification throughout — PXR_NAMESPACE_USING_DIRECTIVE is NOT
// used in this header to avoid polluting includers' namespaces.

namespace omnetpp {
namespace qtenv {

/**
 * @brief Per-viewer stage cache: maps a neutral scene root to a UsdStage and
 * maintains a SdfPath→cObject registry for picking (Decision D4).
 *
 * This class is the M4 scene-builder's home.  In M2 it is a skeleton:
 * getOrBuildStage() ignores the scene root and returns a single, Z-up,
 * metersPerUnit=1 in-memory stage (Decision Q8).  The registry (registerObject /
 * unregisterObject / resolveObject) is fully implemented and mirrors
 * cObjectOsgNode's dual componentId-vs-pointer storage (osgutil.h lines 56–98).
 *
 * This class is NOT a QObject — no Q_OBJECT macro, no moc surprises.
 */
class StageCache
{
public:
    StageCache() = default;
    ~StageCache() = default;

    // Non-copyable; stages are expensive reference-counted resources.
    StageCache(const StageCache&) = delete;
    StageCache& operator=(const StageCache&) = delete;

    /**
     * Returns the UsdStage for the given neutral scene root, building it on
     * demand.
     *
     * M2 skeleton: 'sceneRoot' is not yet inspected; a single in-memory stage
     * is created on the first call and reused thereafter.  The stage is
     * authored with Z-up and metersPerUnit=1 at creation time (Decision Q8).
     *
     * TODO(M4): author prims from the neutral scene tree and maintain a dirty
     * map for incremental updates.  Scene-builder prims additionally carry
     * custom metadata 'omnetpp:objectId' for picking (Decision D4).
     */
    pxr::UsdStageRefPtr getOrBuildStage(cScene3DNode *sceneRoot);

    // --- prim-path → object registry (Decision D4; replaces cObjectOsgNode) ---

    /**
     * Register a cObject for a USD prim path.
     *
     * Mirrors cObjectOsgNode::setObject (osgutil.h lines 86–98): if 'object'
     * is a cComponent, its integer component ID is stored instead of the raw
     * pointer so that a deleted module never leaves a dangling reference.
     */
    void registerObject(const pxr::SdfPath& primPath, const cObject *object);

    /**
     * Remove the binding for primPath (no-op if not registered).
     */
    void unregisterObject(const pxr::SdfPath& primPath);

    /**
     * Resolve a picked prim to its owning cObject by walking ancestor paths.
     *
     * Starts at 'primPath' and walks toward the pseudo-root via
     * SdfPath::GetParentPath(), returning the first registered cObject found.
     * This is the analogue of the OSG NodePath scan in objectsAt().
     *
     * For component bindings (componentId != 0) the current component pointer
     * is retrieved via getSimulation()->getComponent(componentId), mirroring
     * cObjectOsgNode::getObject (osgutil.h lines 81–84).
     *
     * Returns nullptr if no binding is found along the ancestor chain.
     */
    const cObject *resolveObject(const pxr::SdfPath& primPath) const;

    /**
     * Drop all bindings and release the stage RefPtr.
     */
    void clear();

private:
    // Mirrors cObjectOsgNode's dual storage (osgutil.h lines 58–60):
    //   • For cComponents: componentId != 0 and object == nullptr.
    //     Retrieval goes through getSimulation()->getComponent(componentId)
    //     so deleted modules never produce a dangling pointer.
    //   • For plain cObjects: componentId == 0 and object holds the pointer.
    struct ObjectBinding {
        int componentId = 0;          // non-zero iff the bound object is a cComponent
        const cObject *object = nullptr; // valid only when componentId == 0
    };

    std::map<pxr::SdfPath, ObjectBinding> bindings_;
    pxr::UsdStageRefPtr stage_;       // null until first getOrBuildStage() call
};

}  // namespace qtenv
}  // namespace omnetpp

#endif  // __OMNETPP_QTENV_USD_STAGECACHE_H
