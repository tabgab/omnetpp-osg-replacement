//
// Copyright (C) 2006-2015 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/visualizer/usd/util/UsdScene.h"

// Stage up-axis and units helpers
#include <pxr/usd/usdGeom/metrics.h>   // UsdGeomSetStageUpAxis, UsdGeomSetStageMetersPerUnit
#include <pxr/usd/usdGeom/tokens.h>    // UsdGeomTokens->z
#include <pxr/usd/usdGeom/xform.h>     // UsdGeomXform::Define

namespace inet {

namespace usd {

// ---------------------------------------------------------------------------
// TopLevelUsdScene::getStage
// ---------------------------------------------------------------------------

pxr::UsdStageRefPtr TopLevelUsdScene::getStage()
{
    if (!stage) {
        // Create a new in-memory stage.  In-memory stages are not written to
        // disk; they live for the duration of the simulation run and are
        // consumed by the UsdViewer plugin via StageCache.
        stage = pxr::UsdStage::CreateInMemory();

        // Decision Q8: author Z-up and metersPerUnit=1 at stage creation so
        // that every prim authored later is in the correct frame without any
        // per-prim correction.  INET already works in Z-up metres; OSG did too.
        // USD's default is Y-up, 0.01 m/unit (centimetres) — both wrong for us.
        pxr::UsdGeomSetStageUpAxis(stage, pxr::UsdGeomTokens->z);
        pxr::UsdGeomSetStageMetersPerUnit(stage, 1.0);

        // Define the top-level world container.  All simulation geometry lives
        // under /World.  Using a UsdGeomXform lets downstream code apply a
        // global transform if needed (e.g. axis-alignment during model import).
        pxr::UsdGeomXform::Define(stage, pxr::SdfPath("/World"));
    }
    return stage;
}

// ---------------------------------------------------------------------------
// TopLevelUsdScene::getSimulationScene  (instance method)
// ---------------------------------------------------------------------------

SimulationUsdScene *TopLevelUsdScene::getSimulationScene()
{
    if (simulationScene == nullptr) {
        auto stageRef = getStage();  // ensures /World is defined

        // Define the simulation sub-root as a plain Xform.  All INET visualizer
        // content goes under /World/Simulation, mirroring the SimulationScene
        // osg::Group that TopLevelScene::getSimulationScene() finds via
        // FindNodesVisitor (OsgScene.cc lines 22-32).  Here, no traversal is
        // needed: the path is well-known.
        pxr::UsdGeomXform::Define(stageRef, pxr::SdfPath("/World/Simulation"));

        simulationScene = new SimulationUsdScene();
        simulationScene->stage = stageRef;
        simulationScene->path  = pxr::SdfPath("/World/Simulation");
    }
    return simulationScene;
}

// ---------------------------------------------------------------------------
// TopLevelUsdScene::getSimulationScene  (static, cModule* overload)
//
// DESIGN INTENT (M7 — not yet implemented)
// =========================================
// This static overload mirrors TopLevelScene::getSimulationScene(cModule*)
// (OsgScene.cc lines 34-52):
//
//   auto osgCanvas       = module->getOsgCanvas();
//   auto topLevelScene   = ... /* obtain TopLevelUsdScene* from canvas */;
//   if (topLevelScene != nullptr)
//       return topLevelScene->getSimulationScene();
//   else {
//       auto topLevelUsdScene  = new TopLevelUsdScene();
//       auto simulationScene   = topLevelUsdScene->getSimulationScene();
//       /* install topLevelUsdScene on the canvas */
//       osgCanvas->setClearColor(cFigure::Color("#FFFFFF"));
//       osgCanvas->setZNear(0.1);
//       osgCanvas->setZFar(100000);
//       osgCanvas->setCameraManipulatorType(cOsgCanvas::CAM_TERRAIN);
//       return simulationScene;
//   }
//
// OPEN DESIGN POINT — why the body throws instead of half-working
// ================================================================
// cOsgCanvas::setScene / getScene currently expect a cScene3DNode* which,
// UNDER WITH_OSG, is type-aliased to osg::Node* (architecture §2, Decision D1):
//
//   using cScene3DNode = osg::Node;   // while WITH_OSG is set
//
// The alias must go in THIS direction so that existing consumers of
// osg::Node* (OsgScene.cc, OsgViewer, sample models) continue to compile
// during the transition (02-architecture.md §2, "Alias direction matters").
//
// A TopLevelUsdScene is NOT an osg::Node.  Storing it via setScene() while
// WITH_OSG is active would require either:
//   (a) a reinterpret_cast (undefined behaviour), or
//   (b) a parallel storage slot on cOsgCanvas (cScene3DNode* + TopLevelUsdScene*)
//       that only M4 introduces, or
//   (c) a wrapper osg::Node subclass — which re-introduces an OSG dependency
//       that this entire migration aims to remove.
//
// M4 extends cEnvir::refSceneNode / unrefSceneNode with USD-aware semantics
// and adds the parallel storage to cOsgCanvas.  Until M4, any call to this
// static overload would silently store a dangling/wrong pointer.  Throwing
// makes the missing M4 work visible immediately rather than producing a
// hard-to-debug crash at display time.
// ---------------------------------------------------------------------------

SimulationUsdScene *TopLevelUsdScene::getSimulationScene(cModule *module)
{
    // STUB: not implemented — waiting for M4 (cScene3DNode USD-aware semantics).
    // See the design comment above for the full intended implementation and the
    // cScene3DNode transition wrinkle that blocks it.
    throw cRuntimeError(
        "TopLevelUsdScene::getSimulationScene(cModule*): not implemented yet (M7). "
        "Blocked by M4: cScene3DNode must gain USD-aware semantics before a "
        "TopLevelUsdScene* can be safely stored on cOsgCanvas. "
        "See UsdScene.cc design comment for details.");
}

} // namespace usd

} // namespace inet
