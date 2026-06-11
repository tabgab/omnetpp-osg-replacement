//==========================================================================
//  CAMERACONTROLLERS.H - part of
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

#ifndef __OMNETPP_QTENV_USD_CAMERACONTROLLERS_H
#define __OMNETPP_QTENV_USD_CAMERACONTROLLERS_H

// USD ships no camera manipulators; we implement them in pure GfMatrix4d math
// (docs/02-architecture.md §4.1).  These classes are NOT QObjects — no Q_OBJECT
// macro.  Qt event types are only used by pointer so we forward-declare them to
// avoid pulling in heavy Qt headers and to stay QT_NO_KEYWORDS-safe.

#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/matrix4d.h>

// cOsgCanvas::Viewpoint (eye/center/up/valid) lives here; we need the full
// definition because setViewpoint() accepts it by const-ref.
#include "omnetpp/cosgcanvas.h"

// Forward-declare Qt input event types so that virtual handler stubs can
// declare parameters without including <QtGui/QMouseEvent> etc.
// (required for QT_NO_KEYWORDS-safe builds in the plugin Makefile).
class QMouseEvent;
class QWheelEvent;
class QKeyEvent;

namespace omnetpp {
namespace qtenv {

/**
 * @brief Abstract base for USD camera controllers.
 *
 * Replaces the osgGA manipulator hierarchy (docs/02-architecture.md §4.1).
 * OpenUSD provides no camera manipulators; this hierarchy implements the same
 * eye/center/up model on top of pxr::GfMatrix4d.
 *
 * Coordinate convention: Z-up, matching OSG/INET and the stage authored by
 * StageCache::getOrBuildStage() (Decision Q8, docs/02-architecture.md §3).
 *
 * This class is NOT a QObject — no Q_OBJECT, QT_NO_KEYWORDS-safe.
 */
class CameraController
{
protected:
    // Default viewpoint: eye at (3,3,3), looking toward the origin, Z-up.
    // Matches Decision Q8 and mirrors osgGA::OrbitManipulator defaults in the
    // Z-up frame.
    pxr::GfVec3d eye_   {3.0, 3.0, 3.0};
    pxr::GfVec3d center_{0.0, 0.0, 0.0};
    pxr::GfVec3d up_    {0.0, 0.0, 1.0};

public:
    virtual ~CameraController() {}

    /**
     * Apply a cOsgCanvas::Viewpoint hint to the controller.
     *
     * Copies eye/center/up into the GfVec3d state only when vp.valid is true,
     * preserving the Z-up default when the canvas carries no hint.
     * Maps cOsgCanvas::Vec3d {x,y,z} fields directly to pxr::GfVec3d.
     */
    void setViewpoint(const cOsgCanvas::Viewpoint& vp);

    /**
     * Return the world-to-camera view matrix as a pxr::GfMatrix4d.
     *
     * Computed via GfMatrix4d().SetLookAt(eye, center, up).  The result is
     * the viewMatrix argument expected by
     * UsdImagingGLEngine::SetCameraState(viewMatrix, projMatrix).
     */
    pxr::GfMatrix4d getViewMatrix() const;

    // --- Virtual Qt event hooks (M3 implementations in subclasses) ---
    // Declared with empty bodies so the base class is directly instantiable
    // for testing and so subclasses only override what they need.
    virtual void mousePressEvent  (QMouseEvent *) {}
    virtual void mouseReleaseEvent(QMouseEvent *) {}
    virtual void mouseMoveEvent   (QMouseEvent *) {}
    virtual void wheelEvent       (QWheelEvent *) {}
};

// ---------------------------------------------------------------------------
// TrackballController
// ---------------------------------------------------------------------------

/**
 * @brief Trackball-style orbit controller (M3 implementation).
 *
 * Replaces osgGA::TrackballManipulator; allows unrestricted orbit around
 * 'center' by decomposing mouse drag into azimuth/elevation rotations applied
 * to (eye - center) via GfMatrix4d rotation helpers.
 *
 * TODO(M3): orbit math on GfMatrix4d — implement mouseMoveEvent drag-to-rotate,
 * wheelEvent zoom (scale distance), and right-drag pan.
 */
class TrackballController : public CameraController
{
    // Intentionally empty for M2.  M3 adds drag-state members and overrides
    // mousePressEvent / mouseMoveEvent / mouseReleaseEvent / wheelEvent.
};

// ---------------------------------------------------------------------------
// TerrainController  (M5 stub — declaration only)
// ---------------------------------------------------------------------------

// TODO(M5): TerrainController — port TerrainManipulator elevation-clamp logic.
// Eye is never allowed to go below the XY ground plane (z=0); left-drag rotates
// around the center keeping the up-axis fixed; right-drag pans in the XY plane;
// wheel zooms along the look vector.  Based on osgGA::TerrainManipulator with
// the fixed-up behaviour adapted from OverviewManipulator
// (src/qtenv/osg/cameramanipulators.cc).

// ---------------------------------------------------------------------------
// OverviewController  (M5 stub — declaration only)
// ---------------------------------------------------------------------------

// TODO(M5): OverviewController — port OverviewManipulator (src/qtenv/osg/
// cameramanipulators.cc).  Key invariants: up is always +Z (never rolls);
// eye.z is never allowed below center.z (clamped after every rotation);
// right-drag moves the center in the XY plane; same elevation-clamp snap
// as OverviewManipulator::performMovementLeftMouseButton (osgGA orbit +
// eye.z = max(center.z, eye.z) correction, distance preserved).

}  // namespace qtenv
}  // namespace omnetpp

#endif  // __OMNETPP_QTENV_USD_CAMERACONTROLLERS_H
