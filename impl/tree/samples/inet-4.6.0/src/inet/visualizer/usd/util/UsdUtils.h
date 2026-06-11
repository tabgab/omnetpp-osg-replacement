//
// Copyright (C) 2006-2015 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_USDUTILS_H
#define __INET_USDUTILS_H

#include "inet/common/geometry/common/Coord.h"
#include "inet/common/geometry/common/Quaternion.h"

#include "inet/common/INETDefs.h"

// OpenUSD stage and path
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/sdf/path.h>

// Geometry schema prims
#include <pxr/usd/usdGeom/basisCurves.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/gprim.h>

// Shading
#include <pxr/usd/usdShade/material.h>

// Value types
#include <pxr/base/vt/array.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec3d.h>

#include <string>
#include <vector>

// NOTE: No 'using namespace pxr' in headers — all pxr types are fully qualified.
// This mirrors the inet::osg namespace convention (OsgUtils.h lines 26-30)
// but avoids polluting includers with the entire pxr namespace.

namespace inet {

namespace usd {

// ---------------------------------------------------------------------------
// Coordinate helpers  (replaces OsgUtils.h inline toVec3d / toCoord, line 32-33)
// INET and OMNeT++ use Z-up meters; USD stages created by UsdScene are also
// Z-up, metersPerUnit=1 (Decision Q8), so the conversion is a straight copy.
// ---------------------------------------------------------------------------

inline pxr::GfVec3d toGfVec3d(const Coord& c) { return pxr::GfVec3d(c.x, c.y, c.z); }
inline Coord toCoord(const pxr::GfVec3d& v) { return Coord(v[0], v[1], v[2]); }

// ---------------------------------------------------------------------------
// Vertex helpers (replaces OsgUtils.h Vec3Array* overloads, lines 35-36)
// Pure trig — geometry math carries over unchanged because INET is already
// Z-up meters (Decision Q8).
// ---------------------------------------------------------------------------

/// Build a closed circle polygon in the XY-plane at center.z.
/// Returns a flat list of polygonSize vertices (no closing duplicate).
pxr::VtVec3fArray createCircleVertices(const Coord& center, double radius, int polygonSize);  // replaces Vec3Array* createCircleVertices

/// Build an annulus (triangle-strip-ready) vertex list at center.z.
/// Interleaves outer/inner samples: outer[i], inner[i], outer[i+1], inner[i+1], ...
/// Returns 2*(polygonSize+1) vertices, or two zero-vectors when outerRadius==0.
pxr::VtVec3fArray createAnnulusVertices(const Coord& center, double outerRadius, double innerRadius, int polygonSize);  // replaces Vec3Array* createAnnulusVertices

// ---------------------------------------------------------------------------
// Curve prims (replaces createLineGeometry / createLine / createPolylineGeometry
//              / createCircleGeometry, OsgUtils.h lines 38-41)
//
// All curve prims use UsdGeomBasisCurves in "linear" basis with type "nonperiodic"
// (for open polylines) or "periodic" (for closed circles). Points are authored
// at UsdTimeCode::Default() — no time-sampled accumulation (Risk R4).
// ---------------------------------------------------------------------------

/// Single-segment line between two world-space points.
/// Replaces createLineGeometry + createLine (open-ended, no arrowheads).
pxr::UsdGeomBasisCurves createLineCurve(pxr::UsdStagePtr stage,
                                         const pxr::SdfPath& path,
                                         const Coord& start,
                                         const Coord& end);

/// Multi-point open polyline.
/// Replaces createPolylineGeometry + createPolyline (no arrowheads).
pxr::UsdGeomBasisCurves createPolylineCurve(pxr::UsdStagePtr stage,
                                              const pxr::SdfPath& path,
                                              const std::vector<Coord>& coords);

/// Closed circle outline, polygonSize segments.
/// Replaces createCircleGeometry + the LINE_LOOP circle (no arrowheads).
pxr::UsdGeomBasisCurves createCircleCurve(pxr::UsdStagePtr stage,
                                            const pxr::SdfPath& path,
                                            const Coord& center,
                                            double radius,
                                            int polygonSize);

// ---------------------------------------------------------------------------
// Mesh prims (replaces createArrowheadGeometry / createAnnulusGeometry /
//             createQuadGeometry / createPolygonGeometry, OsgUtils.h lines 39-44)
// ---------------------------------------------------------------------------

/// Filled triangle arrowhead mesh.
/// Replaces createArrowheadGeometry / createArrowhead (Node* version).
pxr::UsdGeomMesh createArrowheadMesh(pxr::UsdStagePtr stage,
                                      const pxr::SdfPath& path,
                                      const Coord& start,
                                      const Coord& end,
                                      double width = 10.0,
                                      double height = 20.0);

/// Filled annulus mesh (triangle-strip topology encoded as a quad-strip).
/// Replaces createAnnulusGeometry + the TRIANGLE_STRIP draw-call.
pxr::UsdGeomMesh createAnnulusMesh(pxr::UsdStagePtr stage,
                                    const pxr::SdfPath& path,
                                    const Coord& center,
                                    double outerRadius,
                                    double innerRadius,
                                    int polygonSize);

/// Axis-aligned quad mesh (two triangles) at Z = start.z.
/// Replaces createQuadGeometry + the QUADS draw-call.
pxr::UsdGeomMesh createQuadMesh(pxr::UsdStagePtr stage,
                                 const pxr::SdfPath& path,
                                 const Coord& start,
                                 const Coord& end);

/// Convex (or simple) polygon mesh from an explicit point list.
/// Replaces createPolygonGeometry + the POLYGON draw-call.
pxr::UsdGeomMesh createPolygonMesh(pxr::UsdStagePtr stage,
                                    const pxr::SdfPath& path,
                                    const std::vector<Coord>& points,
                                    const Coord& translation = Coord::ZERO);

// ---------------------------------------------------------------------------
// Text label handle  (replaces osgText::Text — OsgUtils.h line 49)
//
// USD has no text prim. Labels are implemented as a viewer HUD overlay rendered
// via QPainter after each Hydra Render() call (Decision Q6 / architecture §4.2).
// This struct carries the world-space anchor and display properties; the viewer
// collects all TextLabels per frame and projects them to screen coordinates.
// ---------------------------------------------------------------------------

struct INET_API TextLabel {
    Coord       position;   ///< world-space anchor (Z-up, meters)
    std::string text;
    cFigure::Color color;
};

/// Build a TextLabel descriptor.
/// Replaces createText(const char*, const Coord&, const cFigure::Color&).
TextLabel createTextLabel(const char *string, const Coord& position, const cFigure::Color& color);

// ---------------------------------------------------------------------------
// Billboard handle  (replaces createAutoTransform — OsgUtils.h line 51)
//
// USD has no camera-facing constraint (Risk R1). Billboards are implemented as
// regular UsdGeomMesh prims (typically a quad) whose orientation is recomputed
// from the viewer-supplied camera direction on every refreshDisplay() tick.
// BillboardHandle carries only the prim path; the caller is responsible for
// re-orienting the xformOp:orient attribute each frame.
// ---------------------------------------------------------------------------

struct INET_API BillboardHandle {
    pxr::SdfPath primPath;  ///< path to the UsdGeomMesh quad prim in the stage
};

/// Define a billboard quad prim and return a handle to it.
/// Replaces createAutoTransform(Drawable*, AutoRotateMode, bool, const Coord&).
BillboardHandle createBillboard(pxr::UsdStagePtr stage,
                                 const pxr::SdfPath& path,
                                 double width,
                                 double height,
                                 bool autoScaleToScreen,
                                 const Coord& position = Coord::ZERO);

// ---------------------------------------------------------------------------
// Transform prim  (replaces createPositionAttitudeTransform — OsgUtils.h line 52)
// ---------------------------------------------------------------------------

/// Define a UsdGeomXform prim at path with xformOp:translate and xformOp:orient.
/// Replaces createPositionAttitudeTransform(const Coord&, const Quaternion&).
pxr::UsdGeomXform createXform(pxr::UsdStagePtr stage,
                               const pxr::SdfPath& path,
                               const Coord& position,
                               const Quaternion& orientation);

// ---------------------------------------------------------------------------
// Resource resolution  (replaces resolveImageResource — OsgUtils.h line 54)
// Body is OSG-free and ported verbatim from OsgUtils.cc lines 229-240.
// ---------------------------------------------------------------------------

/// Resolve an image resource name to an absolute file path.
/// Tries extensions "", ".png", ".gif", ".jpg" via cComponent::resolveResourcePath.
/// Throws cRuntimeError if not found.
std::string resolveImageResource(const char *imageName, cComponent *context = nullptr);

// ---------------------------------------------------------------------------
// Materials  (replaces createStateSet / createTexture* — OsgUtils.h lines 55-58, 60)
// ---------------------------------------------------------------------------

/// Create a UsdPreviewSurface material prim with a solid diffuseColor.
/// opacity is authored as the material's displayOpacity attribute.
/// cullBackFace is noted in a primvar (enforced by the renderer, e.g. Storm backface cull).
/// Replaces createStateSet(const cFigure::Color&, double, bool).
pxr::UsdShadeMaterial createColorMaterial(pxr::UsdStagePtr stage,
                                            const pxr::SdfPath& path,
                                            const cFigure::Color& color,
                                            double opacity,
                                            bool cullBackFace = true);

/// Create a UsdPreviewSurface material prim that uses a UsdUVTexture.
/// repeat controls wrapS/wrapT (repeat vs. clamp).
/// Replaces createTexture / createTextureFromResource (OsgUtils.h lines 57-58).
pxr::UsdShadeMaterial createTextureMaterial(pxr::UsdStagePtr stage,
                                              const pxr::SdfPath& path,
                                              const char *imagePath,
                                              bool repeat);

// ---------------------------------------------------------------------------
// Line styling  (replaces createLineStateSet — OsgUtils.h line 61)
//
// USD/Storm has no native line-width or stipple equivalent.
// - width: authored as a widths primvar on the BasisCurves prim.
// - dashed/dotted: realized in M8 by curve segmentation (splitting the curve
//   into alternating drawn/gap segments); for now the style is stored but not
//   yet applied (stub, see M8 comment in UsdUtils.cc).
// - overlay: a render-order opinion (e.g. purpose=guide or a dedicated
//   draw-mode attribute) — also deferred to M8.
// ---------------------------------------------------------------------------

/// Apply color, line-style, and width to a BasisCurves prim.
/// Replaces createLineStateSet(const cFigure::Color&, const cFigure::LineStyle&, double, bool).
void setLineStyle(const pxr::UsdGeomBasisCurves& curves,
                  const cFigure::Color& color,
                  cFigure::LineStyle style,
                  double width,
                  bool overlay = false);

// ---------------------------------------------------------------------------
// UsdLineNode  (replaces LineNode : Group — OsgUtils.h lines 64-88)
//
// Instead of child indices into an osg::Group, UsdLineNode holds SdfPaths to
// the line curve prim and optional arrowhead mesh prims.  Point re-authoring
// overwrites the 'points' attribute at UsdTimeCode::Default() — never
// accumulates time samples (Risk R4).
// ---------------------------------------------------------------------------

class INET_API UsdLineNode
{
  protected:
    pxr::UsdStageWeakPtr    stage;
    pxr::SdfPath            linePath;
    pxr::SdfPath            startArrowheadPath;
    pxr::SdfPath            endArrowheadPath;
    double                  lineWidth;
    cFigure::Arrowhead      startArrowhead;
    cFigure::Arrowhead      endArrowhead;

  public:
    /// Construct and define all prims (line + optional arrowheads) under parentPath.
    UsdLineNode(pxr::UsdStagePtr stage,
                const pxr::SdfPath& parentPath,
                const Coord& start,
                const Coord& end,
                cFigure::Arrowhead startArrowhead,
                cFigure::Arrowhead endArrowhead,
                double lineWidth);

    /// Update the start point: re-authors the 'points' attribute on the line prim
    /// and repositions the start arrowhead mesh (if present).
    /// Overwrites the attribute at UsdTimeCode::Default() — never accumulates
    /// time samples (Risk R4).
    void setStart(const Coord& start);

    /// Update the end point: re-authors the 'points' attribute on the line prim
    /// and repositions the end arrowhead mesh (if present).
    void setEnd(const Coord& end);
};

} // namespace usd

} // namespace inet

#endif // __INET_USDUTILS_H
