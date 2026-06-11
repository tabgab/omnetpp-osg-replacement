//
// Copyright (C) 2006-2015 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_USDSCENE_H
#define __INET_USDSCENE_H

#include "inet/common/INETDefs.h"

// OpenUSD stage and path — fully qualified (no 'using namespace pxr' in headers)
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/sdf/path.h>

// NOTE: No NodeVisitor equivalent needed — the USD stage is queried by SdfPath,
// so FindNodesVisitor<T> has no analogue here.  This mirrors the intent of
// OsgScene.h but without the traversal machinery (osg::NodeVisitor removed).

namespace inet {

namespace usd {

/**
 * Carries the stage-root path of the simulation sub-hierarchy.
 *
 * USD analogue of inet::osg::SimulationScene (OsgScene.h lines 41-43).
 * Instead of being an osg::Group subclass, this is a plain data struct:
 * the USD stage is the authoritative container; prim references are
 * lightweight path values.
 *
 * Default prim path: /World/Simulation
 */
class INET_API SimulationUsdScene
{
  public:
    pxr::UsdStageRefPtr stage;  ///< shared ownership of the stage (same ptr as TopLevelUsdScene)
    pxr::SdfPath        path;   ///< path to the UsdGeomXform prim (default: /World/Simulation)
};

/**
 * Owns the in-memory USD stage that represents the entire simulation world.
 *
 * USD analogue of inet::osg::TopLevelScene (OsgScene.h lines 49-58).
 * The stage is created lazily on first call to getStage().  It is authored with:
 *   - upAxis = Z    (Decision Q8 — INET is Z-up)
 *   - metersPerUnit = 1.0  (Decision Q8 — INET works in metres)
 *   - a UsdGeomXform prim at /World (top-level container, all simulation content lives here)
 *
 * No osg::Group base class, no NodeVisitor.
 */
class INET_API TopLevelUsdScene
{
  protected:
    pxr::UsdStageRefPtr    stage;
    SimulationUsdScene    *simulationScene = nullptr;

  public:
    /// Return (and lazily create) the in-memory stage.
    /// On first call: UsdStage::CreateInMemory(), Z-up, metersPerUnit=1,
    /// defines UsdGeomXform at /World.
    pxr::UsdStageRefPtr getStage();

    /// Return (and lazily define) the SimulationUsdScene at /World/Simulation.
    /// Calls getStage() internally.
    SimulationUsdScene *getSimulationScene();

    /**
     * Module-level entry point mirroring OsgScene.cc lines 34-52.
     *
     * STUB — throws cRuntimeError.  See UsdScene.cc for the full design comment.
     *
     * M7 intent: obtain (or create) the TopLevelUsdScene* associated with
     * cModule::getOsgCanvas(), install hints (clearColor #FFFFFF, zNear 0.1,
     * zFar 100000, CAM_TERRAIN), and return the SimulationUsdScene*.
     *
     * This overload is blocked by an open design point: how a TopLevelUsdScene*
     * is carried inside cScene3DNode* during the WITH_OSG transition.  The
     * implementation is deferred to M4 when refSceneNode gets USD-aware semantics.
     * See UsdScene.cc for the rationale.
     */
    static SimulationUsdScene *getSimulationScene(cModule *module);
};

} // namespace usd

} // namespace inet

#endif // __INET_USDSCENE_H
