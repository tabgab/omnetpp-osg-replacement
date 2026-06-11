//==========================================================================
//  STAGECACHE.CC - part of
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

#include "stagecache.h"

#include "omnetpp/csimulation.h"    // getSimulation(), cSimulation::getComponent()
#include "omnetpp/ccomponent.h"     // cComponent, getId()

namespace omnetpp {
namespace qtenv {

// ---------------------------------------------------------------------------
// getOrBuildStage
// ---------------------------------------------------------------------------

pxr::UsdStageRefPtr StageCache::getOrBuildStage(cScene3DNode * /*sceneRoot*/)
{
    if (!stage_) {
        // Create a fresh in-memory stage.  Decision Q8 (docs/02-architecture.md §3):
        // author Z-up and metersPerUnit=1 on every stage we create so that
        // OSG/INET's coordinate conventions are preserved — USD defaults are
        // Y-up and 0.01 m/unit (centimetres) and would rotate/scale everything.
        stage_ = pxr::UsdStage::CreateInMemory();
        pxr::UsdGeomSetStageUpAxis(stage_, pxr::UsdGeomTokens->z);
        pxr::UsdGeomSetStageMetersPerUnit(stage_, 1.0);

        // TODO(M4): author prims from the neutral scene tree received in
        // 'sceneRoot' and maintain a dirty map for incremental updates.
        // Scene-builder prims additionally carry custom metadata
        // 'omnetpp:objectId' (Decision D4, docs/02-architecture.md §5) so the
        // picking path can locate the bound cObject without a full ancestor
        // walk.  For now the stage is empty; callers can render it but picking
        // will always return nullptr.
    }
    return stage_;
}

// ---------------------------------------------------------------------------
// registerObject
// ---------------------------------------------------------------------------

void StageCache::registerObject(const pxr::SdfPath& primPath, const cObject *object)
{
    // Mirror cObjectOsgNode::setObject (include/omnetpp/osgutil.h lines 86–98):
    // when the object is a cComponent store its integer ID so that a deleted
    // module never leaves a dangling raw pointer in the registry.
    ObjectBinding binding;
    if (const cComponent *component = dynamic_cast<const cComponent*>(object)) {
        binding.componentId = component->getId();
        binding.object = nullptr;
    }
    else {
        binding.componentId = 0;
        binding.object = object;
    }
    bindings_[primPath] = binding;
}

// ---------------------------------------------------------------------------
// unregisterObject
// ---------------------------------------------------------------------------

void StageCache::unregisterObject(const pxr::SdfPath& primPath)
{
    bindings_.erase(primPath);
}

// ---------------------------------------------------------------------------
// resolveObject
// ---------------------------------------------------------------------------

const cObject *StageCache::resolveObject(const pxr::SdfPath& primPath) const
{
    // Walk from the picked prim up toward the pseudo-root, checking each
    // ancestor for a registered binding.  This is the analogue of the OSG
    // NodePath scan used by objectsAt() with cObjectOsgNode (see
    // docs/02-architecture.md §5 and osgutil.h lines 81–84).
    for (pxr::SdfPath p = primPath;
         p != pxr::SdfPath::AbsoluteRootPath();
         p = p.GetParentPath())
    {
        auto it = bindings_.find(p);
        if (it != bindings_.end()) {
            const ObjectBinding& b = it->second;
            if (b.componentId != 0) {
                // Mirror cObjectOsgNode::getObject (osgutil.h lines 81–84):
                // resolve through the live simulation component table; returns
                // nullptr if the component has since been deleted.
                return getSimulation()->getComponent(b.componentId);
            }
            else {
                return b.object;
            }
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// clear
// ---------------------------------------------------------------------------

void StageCache::clear()
{
    bindings_.clear();
    // Release the stage RefPtr; next getOrBuildStage() call will allocate a
    // fresh one with the correct axis/unit metadata.
    stage_ = pxr::UsdStageRefPtr();
}

}  // namespace qtenv
}  // namespace omnetpp
