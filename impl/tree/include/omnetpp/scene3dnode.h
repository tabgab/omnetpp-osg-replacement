//==========================================================================
//   SCENE3DNODE.H  -  header for
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

#ifndef __OMNETPP_SCENE3DNODE_H
#define __OMNETPP_SCENE3DNODE_H

// Pull in WITH_OSG (and other feature flags) via the verified chain:
//   simkerneldefs.h (line 23) -> platdep/platdefs.h (line 29) ->
//   omnetpp/platdep/config.h  which #defines / #undefs WITH_OSG.
// Do NOT include platdep/config.h directly — simkerneldefs.h is the
// documented, stable entry point for simulation-kernel feature flags.
#include "simkerneldefs.h"

#ifdef WITH_OSG

// Transitional: the renderer-neutral scene-root handle is a type alias for
// osg::Node while WITH_OSG is set. This keeps all existing osg::Node*
// consumers (INET OsgScene.cc, qtenv osgviewer.cc downcasts, sample models)
// compiling unchanged — an osg::Node* IS a cScene3DNode* and vice-versa, with
// no cast required. The alias is removed together with WITH_OSG at M12.
//
// The simulation kernel never dereferences the scene root; cOsgCanvas only
// stores the pointer and ref/unrefs it through cEnvir::refSceneNode /
// unrefSceneNode. An opaque handle therefore suffices for the kernel, and the
// alias direction (cScene3DNode -> osg::Node, NOT the reverse) ensures the
// OSG-facing type remains the canonical one during the transition so that no
// existing osg::Node* site needs a cast or a #ifdef.
//
// @ingroup OSG

namespace osg { class Node; }
namespace omnetpp { using cScene3DNode = osg::Node; }

#else  // !WITH_OSG

/**
 * @brief Renderer-neutral, opaque handle for the 3-D scene root.
 *
 * The simulation kernel never dereferences the scene root: cOsgCanvas stores
 * only a pointer to it and forwards ref/unref calls to the environment via
 * cEnvir::refSceneNode / cEnvir::unrefSceneNode. An opaque, incomplete type
 * therefore suffices — only pointers to cScene3DNode are passed around at the
 * kernel level.
 *
 * In the USD end-state (WITH_USD, M3+) a UsdStage handle lives behind this
 * pointer, managed by the oppqtenv-usd plugin. Until that plugin is loaded the
 * pointer is always null.
 *
 * cScene3DNode is deliberately never defined in this header: translation units
 * that must dereference it must include the appropriate USD/viewer header
 * instead. This keeps the kernel library free of any OpenUSD linker
 * dependency — mirroring the original design constraint that prevented OSG
 * headers from being included in cosgcanvas.h.
 *
 * @ingroup OSG
 */
namespace omnetpp { class cScene3DNode; }

#endif  // WITH_OSG

#endif  // __OMNETPP_SCENE3DNODE_H
