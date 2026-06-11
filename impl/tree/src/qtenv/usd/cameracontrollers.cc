//==========================================================================
//  CAMERACONTROLLERS.CC - part of
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

#include "cameracontrollers.h"

namespace omnetpp {
namespace qtenv {

// ---------------------------------------------------------------------------
// CameraController::setViewpoint
// ---------------------------------------------------------------------------

void CameraController::setViewpoint(const cOsgCanvas::Viewpoint& vp)
{
    // Only apply the hint when the canvas has set a valid viewpoint;
    // otherwise keep the Z-up default (eye={3,3,3}, center={0,0,0}, up={0,0,1}).
    if (!vp.valid)
        return;

    // Map cOsgCanvas::Vec3d {x, y, z} fields to pxr::GfVec3d.
    // cosgcanvas.h lines 69-73: Vec3d has public members x, y, z (double).
    eye_    = pxr::GfVec3d(vp.eye.x,    vp.eye.y,    vp.eye.z);
    center_ = pxr::GfVec3d(vp.center.x, vp.center.y, vp.center.z);
    up_     = pxr::GfVec3d(vp.up.x,     vp.up.y,     vp.up.z);
}

// ---------------------------------------------------------------------------
// CameraController::getViewMatrix
// ---------------------------------------------------------------------------

pxr::GfMatrix4d CameraController::getViewMatrix() const
{
    // GfMatrix4d::SetLookAt(eye, center, up) computes the world-to-camera
    // transformation matrix — the same convention as OpenGL's gluLookAt and
    // the viewMatrix parameter of UsdImagingGLEngine::SetCameraState().
    //
    // Note: GfMatrix4d::SetLookAt returns a reference to *this, so we
    // construct a default matrix and then call SetLookAt on it.
    return pxr::GfMatrix4d().SetLookAt(eye_, center_, up_);
}

// ---------------------------------------------------------------------------
// TrackballController — M2 stub; no overrides yet (M3 implements orbit math).
// ---------------------------------------------------------------------------

// (No additional definitions needed for the M2 stub; the base-class virtuals
//  provide the empty event-handler bodies.  M3 adds drag-state members and
//  overrides mouse*Event / wheelEvent here.)

}  // namespace qtenv
}  // namespace omnetpp
