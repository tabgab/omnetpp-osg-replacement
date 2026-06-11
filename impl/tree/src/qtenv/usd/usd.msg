//==============================================================
//   USD.MSG  - part of
//                     OMNeT++/OMNEST
//            Discrete System Simulation in C++
//
//==============================================================

//--------------------------------------------------------------
// Copyright (C) 2017 OpenSim Ltd.
//
//  This file is distributed WITHOUT ANY WARRANTY. See the file
//  `license' for details on this and other legal matters.
//--------------------------------------------------------------

// NOTE: Only ONE 3D plugin library is ever loaded per process (the loader in
// iosgviewer.cc stops at the first factory that registers), so the duplicate
// Register_Enum calls in osg.msg and usd.msg can never collide at runtime.

cplusplus
{{

namespace omnetpp {

// Register_Enum for both cOsgCanvas enums so that the Qtenv inspector's
// Fields tab shows human-readable enum names in a USD-only build (Risk R13).
//
// DELIBERATE DIVERGENCE FROM osg.msg: osg.msg line 20 omits CAM_TERRAIN and
// CAM_OVERVIEW from the CameraManipulatorType registration (a silent
// oversight). This file registers all five enumerators so that the inspector
// displays them correctly regardless of which manipulator is active.
Register_Enum(cOsgCanvas::ViewerStyle, (cOsgCanvas::STYLE_GENERIC, cOsgCanvas::STYLE_EARTH));
Register_Enum(cOsgCanvas::CameraManipulatorType, (cOsgCanvas::CAM_AUTO, cOsgCanvas::CAM_TERRAIN, cOsgCanvas::CAM_OVERVIEW, cOsgCanvas::CAM_TRACKBALL, cOsgCanvas::CAM_EARTH));

} // namespace omnetpp

#include "qtenv/qtenvdefs.h"
#include "omnetpp/cosgcanvas.h"
// NOTE: omnetpp/osgutil.h and all <osg/*> headers are intentionally NOT
// included here.  The osgNode_* helper functions from osg.msg (lines 23-54)
// are OSG-specific and have no analogue in the USD plugin.

}}

namespace omnetpp;

enum cOsgCanvas::ViewerStyle;
enum cOsgCanvas::CameraManipulatorType;

struct cOsgCanvas::Vec3d
{
    @existingClass;
    @descriptor(false);
    @opaque;
    @toString(.str());
    @toValue(.str());
    // TODO fromstr for Vec3d
}

struct cOsgCanvas::Viewpoint
{
    @existingClass;
    bool valid @editable;
    cOsgCanvas::Vec3d eye;
    cOsgCanvas::Vec3d center;
    cOsgCanvas::Vec3d up;
}

struct cOsgCanvas::EarthViewpoint
{
    @existingClass;
    bool valid @editable;
    double longitude @editable; // TODO grouping?
    double latitude @editable;
    double altitude @editable;
    double heading @editable;
    double pitch @editable;
    double range @editable;
}

class cOsgCanvas extends cOwnedObject
{
    @existingClass;
    @overwritePreviousDefinition; // in sim_std.msg
    // scene root (cScene3DNode*) deliberately not exposed here: under WITH_USD
    // it is an opaque handle; stage/prim tree browsing is added in M4 via the
    // StageCache.
    int viewerStyle @editable @enum(omnetpp::cOsgCanvas::ViewerStyle) @group(viewer hints);
    cFigure::Color clearColor @editable @group(viewer hints);
    int cameraManipulatorType @editable @enum(omnetpp::cOsgCanvas::CameraManipulatorType) @group(viewer hints);
    double fieldOfViewAngle @editable @group(viewer hints);
    double zNear @editable @group(viewer hints);
    double zFar @editable @group(viewer hints);
    cOsgCanvas::Viewpoint genericViewpoint @toValue(omnetpp::toAnyPtr(&$));
    cOsgCanvas::EarthViewpoint earthViewpoint @toValue(omnetpp::toAnyPtr(&$));
}

// NOTE: cObjectOsgNode (osg.msg lines 108-113) and the trailing 'namespace osg;'
// section with class Node/Group descriptors (osg.msg lines 115-128) are
// intentionally omitted.  Those classes browse the OSG node tree and have no
// USD analogue yet; OSG node-tree browsing will be replaced by stage/prim tree
// browsing introduced in M4 via the StageCache.
