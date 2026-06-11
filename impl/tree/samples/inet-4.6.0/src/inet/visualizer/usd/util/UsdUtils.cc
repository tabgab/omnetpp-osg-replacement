//
// Copyright (C) 2006-2015 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/visualizer/usd/util/UsdUtils.h"

#include <cmath>

// pxr geometry tokens needed for curve/mesh authoring
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>

// Shading schema tokens and connectable API
#include <pxr/usd/usdShade/tokens.h>
#include <pxr/usd/usdShade/connectableAPI.h>
#include <pxr/usd/usdShade/shader.h>

// GfQuatd for orientation
#include <pxr/base/gf/quatd.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/value.h>

// VtArray integer types
#include <pxr/base/vt/array.h>

namespace inet {

namespace usd {

// ===========================================================================
// Vertex helpers — fully ported trig loops from OsgUtils.cc lines 22-55
// ===========================================================================

pxr::VtVec3fArray createCircleVertices(const Coord& center, double radius, int polygonSize)
{
    // Ported from OsgUtils.cc createCircleVertices (lines 22-33).
    // Pure trigonometry; no OSG types.  Z-up frame: circle lies in XY-plane
    // at constant center.z (Decision Q8 — geometry math unchanged).
    pxr::VtVec3fArray vertices;
    vertices.reserve(polygonSize);
    double theta, px, py;
    for (int i = 1; i <= polygonSize; i++) {
        theta = 2.0 * M_PI / polygonSize * i;
        px = center.x + radius * cos(theta);
        py = center.y + radius * sin(theta);
        vertices.push_back(pxr::GfVec3f((float)px, (float)py, (float)center.z));
    }
    return vertices;
}

pxr::VtVec3fArray createAnnulusVertices(const Coord& center, double outerRadius, double innerRadius, int polygonSize)
{
    // Ported from OsgUtils.cc createAnnulusVertices (lines 35-55).
    // Interleaves outer/inner samples for triangle-strip topology.
    // Two degenerate zero-vectors when outerRadius == 0.
    pxr::VtVec3fArray vertices;
    if (outerRadius > 0) {
        vertices.reserve(2 * (polygonSize + 1));
        double theta, px, py;
        for (int i = 0; i <= polygonSize; i++) {
            theta = 2.0 * M_PI / polygonSize * i;
            px = center.x + outerRadius * cos(theta);
            py = center.y + outerRadius * sin(theta);
            vertices.push_back(pxr::GfVec3f((float)px, (float)py, (float)center.z));
            px = center.x + innerRadius * cos(theta);
            py = center.y + innerRadius * sin(theta);
            vertices.push_back(pxr::GfVec3f((float)px, (float)py, (float)center.z));
        }
    }
    else {
        vertices.push_back(pxr::GfVec3f(0.0f, 0.0f, 0.0f));
        vertices.push_back(pxr::GfVec3f(0.0f, 0.0f, 0.0f));
    }
    return vertices;
}

// ===========================================================================
// Resource resolution — ported verbatim from OsgUtils.cc lines 229-240.
// OSG-free: uses only OMNeT++ runtime APIs.
// ===========================================================================

std::string resolveImageResource(const char *imageName, cComponent *context)
{
    if (context == nullptr)
        context = cSimulation::getActiveSimulation()->getContextModule();
    std::string path;
    for (auto ext : { "", ".png", ".gif", ".jpg" }) {
        path = context->resolveResourcePath((std::string(imageName) + ext).c_str());
        if (!path.empty())
            return path;
    }
    throw cRuntimeError("Image '%s' not found", imageName);
}

// ===========================================================================
// Curve prims
// ===========================================================================

pxr::UsdGeomBasisCurves createLineCurve(pxr::UsdStagePtr stage,
                                          const pxr::SdfPath& path,
                                          const Coord& start,
                                          const Coord& end)
{
    // M7 intent:
    //   auto curves = pxr::UsdGeomBasisCurves::Define(stage, path);
    //   curves.CreateTypeAttr().Set(pxr::UsdGeomTokens->linear);
    //   curves.CreateBasisAttr().Set(pxr::UsdGeomTokens->bezier); // unused for linear
    //   curves.CreateWrapAttr().Set(pxr::UsdGeomTokens->nonperiodic);
    //   pxr::VtVec3fArray pts = { toGfVec3d(start), toGfVec3d(end) };
    //   curves.CreatePointsAttr().Set(pxr::VtValue(pts), pxr::UsdTimeCode::Default());
    //   pxr::VtIntArray curveVertexCounts = { 2 };
    //   curves.CreateCurveVertexCountsAttr().Set(curveVertexCounts);
    //   return curves;
    throw cRuntimeError("UsdUtils::createLineCurve(): not implemented yet (milestone M7)");
}

pxr::UsdGeomBasisCurves createPolylineCurve(pxr::UsdStagePtr stage,
                                               const pxr::SdfPath& path,
                                               const std::vector<Coord>& coords)
{
    // M7 intent:
    //   Similar to createLineCurve, but with N points (coords.size()).
    //   type=linear, wrap=nonperiodic, curveVertexCounts=[N].
    //   Convert each Coord to GfVec3f and push into VtVec3fArray.
    throw cRuntimeError("UsdUtils::createPolylineCurve(): not implemented yet (milestone M7)");
}

pxr::UsdGeomBasisCurves createCircleCurve(pxr::UsdStagePtr stage,
                                             const pxr::SdfPath& path,
                                             const Coord& center,
                                             double radius,
                                             int polygonSize)
{
    // M7 intent:
    //   auto curves = pxr::UsdGeomBasisCurves::Define(stage, path);
    //   curves.CreateTypeAttr().Set(pxr::UsdGeomTokens->linear);
    //   curves.CreateWrapAttr().Set(pxr::UsdGeomTokens->periodic);  // closed loop
    //   auto pts = createCircleVertices(center, radius, polygonSize);
    //   curves.CreatePointsAttr().Set(pts, pxr::UsdTimeCode::Default());
    //   pxr::VtIntArray curveVertexCounts = { polygonSize };
    //   curves.CreateCurveVertexCountsAttr().Set(curveVertexCounts);
    //   return curves;
    throw cRuntimeError("UsdUtils::createCircleCurve(): not implemented yet (milestone M7)");
}

// ===========================================================================
// Mesh prims
// ===========================================================================

pxr::UsdGeomMesh createArrowheadMesh(pxr::UsdStagePtr stage,
                                       const pxr::SdfPath& path,
                                       const Coord& start,
                                       const Coord& end,
                                       double width,
                                       double height)
{
    // M7 intent:
    //   Port createArrowheadGeometry logic (OsgUtils.cc lines 71-103) to USD:
    //   Compute direction = end - start, normalise, find a perpendicular,
    //   build three vertex positions (tip, base-left, base-right), define a
    //   UsdGeomMesh with faceVertexCounts=[3], faceVertexIndices=[0,1,2].
    //   Author points at UsdTimeCode::Default().
    throw cRuntimeError("UsdUtils::createArrowheadMesh(): not implemented yet (milestone M7)");
}

pxr::UsdGeomMesh createAnnulusMesh(pxr::UsdStagePtr stage,
                                     const pxr::SdfPath& path,
                                     const Coord& center,
                                     double outerRadius,
                                     double innerRadius,
                                     int polygonSize)
{
    // M7 intent:
    //   Use createAnnulusVertices() to obtain the interleaved outer/inner ring.
    //   Build a UsdGeomMesh triangle-strip: each quad (4 verts from the strip)
    //   becomes two triangles.  faceVertexCounts is a flat array of 3s,
    //   faceVertexIndices encodes each triangle. Author points at Default().
    throw cRuntimeError("UsdUtils::createAnnulusMesh(): not implemented yet (milestone M7)");
}

pxr::UsdGeomMesh createQuadMesh(pxr::UsdStagePtr stage,
                                  const pxr::SdfPath& path,
                                  const Coord& start,
                                  const Coord& end)
{
    // M7 intent:
    //   Four corners at (start.x,start.y), (start.x,end.y),
    //                   (end.x,end.y),   (end.x,start.y) at z=start.z.
    //   UsdGeomMesh with faceVertexCounts=[4], faceVertexIndices=[0,1,2,3],
    //   subdivisionScheme=none. Author points at Default().
    throw cRuntimeError("UsdUtils::createQuadMesh(): not implemented yet (milestone M7)");
}

pxr::UsdGeomMesh createPolygonMesh(pxr::UsdStagePtr stage,
                                     const pxr::SdfPath& path,
                                     const std::vector<Coord>& points,
                                     const Coord& translation)
{
    // M7 intent:
    //   Translate each point by 'translation', build VtVec3fArray.
    //   UsdGeomMesh with faceVertexCounts=[N], faceVertexIndices=[0..N-1],
    //   subdivisionScheme=none. Author points at Default().
    //   Matches createPolygonGeometry (OsgUtils.cc lines 148-157).
    throw cRuntimeError("UsdUtils::createPolygonMesh(): not implemented yet (milestone M7)");
}

// ===========================================================================
// Text label
// ===========================================================================

TextLabel createTextLabel(const char *string, const Coord& position, const cFigure::Color& color)
{
    // M7 intent (actually this is a data-only struct construction — already trivial,
    // but kept as a function for API symmetry with createText):
    //   TextLabel lbl;
    //   lbl.text     = string;
    //   lbl.position = position;
    //   lbl.color    = color;
    //   return lbl;
    // Note: the viewer collects TextLabels and renders them via QPainter HUD
    // overlay (Decision Q6, architecture §4.2).  No USD prim is created.
    throw cRuntimeError("UsdUtils::createTextLabel(): not implemented yet (milestone M7)");
}

// ===========================================================================
// Billboard
// ===========================================================================

BillboardHandle createBillboard(pxr::UsdStagePtr stage,
                                  const pxr::SdfPath& path,
                                  double width,
                                  double height,
                                  bool autoScaleToScreen,
                                  const Coord& position)
{
    // M7 intent:
    //   Define a UsdGeomMesh quad centred at 'position' in the XY-plane
    //   (width x height).  The viewer re-orients its xformOp:orient each frame
    //   to face the camera (Risk R1 — no USD camera-facing constraint).
    //   autoScaleToScreen is stored as a custom primvar for the viewer to read.
    //   Return BillboardHandle{ path }.
    throw cRuntimeError("UsdUtils::createBillboard(): not implemented yet (milestone M7)");
}

// ===========================================================================
// Transform prim
// ===========================================================================

pxr::UsdGeomXform createXform(pxr::UsdStagePtr stage,
                                const pxr::SdfPath& path,
                                const Coord& position,
                                const Quaternion& orientation)
{
    // M7 intent:
    //   auto xform = pxr::UsdGeomXform::Define(stage, path);
    //   xform.AddTranslateOp().Set(toGfVec3d(position), pxr::UsdTimeCode::Default());
    //   // Quaternion: INET Quaternion stores (s, v) where q = s + v.x*i + v.y*j + v.z*k
    //   pxr::GfQuatd q(orientation.s,
    //                  pxr::GfVec3d(orientation.v.x, orientation.v.y, orientation.v.z));
    //   xform.AddOrientOp().Set(q, pxr::UsdTimeCode::Default());
    //   return xform;
    // Replaces createPositionAttitudeTransform (OsgUtils.cc lines 221-227).
    throw cRuntimeError("UsdUtils::createXform(): not implemented yet (milestone M7)");
}

// ===========================================================================
// Materials
// ===========================================================================

pxr::UsdShadeMaterial createColorMaterial(pxr::UsdStagePtr stage,
                                             const pxr::SdfPath& path,
                                             const cFigure::Color& color,
                                             double opacity,
                                             bool cullBackFace)
{
    // M7 intent:
    //   auto material = pxr::UsdShadeMaterial::Define(stage, path);
    //   auto shader   = pxr::UsdShadeShader::Define(stage, path.AppendChild(TfToken("PBR")));
    //   shader.CreateIdAttr().Set(TfToken("UsdPreviewSurface"));
    //   shader.CreateInput(TfToken("diffuseColor"), pxr::SdfValueTypeNames->Color3f)
    //         .Set(pxr::GfVec3f(color.red/255.f, color.green/255.f, color.blue/255.f));
    //   shader.CreateInput(TfToken("opacity"), pxr::SdfValueTypeNames->Float)
    //         .Set((float)opacity);
    //   material.CreateSurfaceOutput().ConnectToSource(
    //       shader.ConnectableAPI(), TfToken("surface"));
    //   if (cullBackFace) {
    //       // Author displayOpacity + a custom primvar hint; Storm respects
    //       // UsdGeomGprim's doubleSided=false (the default) for back-face culling.
    //   }
    //   return material;
    // Replaces createStateSet (OsgUtils.cc lines 273-291).
    throw cRuntimeError("UsdUtils::createColorMaterial(): not implemented yet (milestone M7)");
}

pxr::UsdShadeMaterial createTextureMaterial(pxr::UsdStagePtr stage,
                                               const pxr::SdfPath& path,
                                               const char *imagePath,
                                               bool repeat)
{
    // M7 intent:
    //   UsdShadeMaterial + UsdShadeShader(UsdPreviewSurface) + UsdShadeShader(UsdUVTexture):
    //   texShader.CreateInput("file", Asset).Set(SdfAssetPath(imagePath));
    //   texShader.CreateInput("wrapS", Token).Set(repeat ? "repeat" : "clamp");
    //   texShader.CreateInput("wrapT", Token).Set(repeat ? "repeat" : "clamp");
    //   texShader.CreateOutput("rgb", Color3f) connected to UsdPreviewSurface diffuseColor.
    //   UsdPrimvarReader_float2 for st primvar.
    // Replaces createTexture + createTextureFromResource (OsgUtils.cc lines 255-271).
    throw cRuntimeError("UsdUtils::createTextureMaterial(): not implemented yet (milestone M7)");
}

// ===========================================================================
// Line style
// ===========================================================================

void setLineStyle(const pxr::UsdGeomBasisCurves& curves,
                  const cFigure::Color& color,
                  cFigure::LineStyle style,
                  double width,
                  bool overlay)
{
    // M7/M8 intent:
    //   1. Color: bind a UsdShadeMaterial (createColorMaterial) to the curves prim.
    //   2. Width: author the 'widths' primvar (VtFloatArray{(float)width}) with
    //      interpolation=constant.
    //   3. Dashed/Dotted style (M8): realized by curve segmentation —
    //      split the single polyline into alternating drawn/gap segments so that
    //      the gap segments have zero widths or are removed. The exact dash length
    //      is a configurable parameter deferred to M8.
    //   4. Overlay: author purpose=guide or a renderOrder opinion so the curves
    //      render on top of geometry (analogous to OSG TRANSPARENT_BIN + depth-off).
    //      Exact Storm mechanism to be confirmed in M8.
    // Replaces createLineStateSet (OsgUtils.cc lines 293-328).
    throw cRuntimeError("UsdUtils::setLineStyle(): not implemented yet (milestone M7/M8)");
}

// ===========================================================================
// UsdLineNode
// ===========================================================================

UsdLineNode::UsdLineNode(pxr::UsdStagePtr stage_,
                           const pxr::SdfPath& parentPath,
                           const Coord& start,
                           const Coord& end,
                           cFigure::Arrowhead startArrowhead_,
                           cFigure::Arrowhead endArrowhead_,
                           double lineWidth_)
    : stage(stage_),
      lineWidth(lineWidth_),
      startArrowhead(startArrowhead_),
      endArrowhead(endArrowhead_)
{
    // M7 intent:
    //   linePath           = parentPath.AppendChild(TfToken("line"));
    //   startArrowheadPath = parentPath.AppendChild(TfToken("startArrowhead"));
    //   endArrowheadPath   = parentPath.AppendChild(TfToken("endArrowhead"));
    //
    //   createLineCurve(stage_, linePath, start, end);
    //   if (startArrowhead_)
    //       createArrowheadMesh(stage_, startArrowheadPath, end, start,
    //                           10 + 2*lineWidth_, 20 + 2*lineWidth_);
    //   if (endArrowhead_)
    //       createArrowheadMesh(stage_, endArrowheadPath, start, end,
    //                           10 + 2*lineWidth_, 20 + 2*lineWidth_);
    //
    // Note: no addChild / osg ref-counting — paths are value-typed (Risk R4).
    // Replaces LineNode::LineNode (OsgUtils.cc lines 330-343).
    throw cRuntimeError("UsdLineNode::UsdLineNode(): not implemented yet (milestone M7)");
}

void UsdLineNode::setStart(const Coord& start)
{
    // M7 intent:
    //   auto curves = pxr::UsdGeomBasisCurves(stage->GetPrimAtPath(linePath));
    //   auto pointsAttr = curves.GetPointsAttr();
    //   pxr::VtVec3fArray pts;
    //   pointsAttr.Get(&pts, pxr::UsdTimeCode::Default());
    //   pts[0] = pxr::GfVec3f((float)start.x, (float)start.y, (float)start.z);
    //   pointsAttr.Set(pts, pxr::UsdTimeCode::Default());   // overwrite, never accumulate
    //
    //   if (startArrowhead) {
    //       // Re-define (overwrite) the arrowhead mesh prim at startArrowheadPath
    //       // pointing from the new start toward end (read end from pts[1]).
    //       // Overwrite: prim is already defined; Set() the points attr directly.
    //   }
    // Replaces LineNode::setStart (OsgUtils.cc lines 345-358).
    throw cRuntimeError("UsdLineNode::setStart(): not implemented yet (milestone M7)");
}

void UsdLineNode::setEnd(const Coord& end)
{
    // M7 intent:
    //   auto curves = pxr::UsdGeomBasisCurves(stage->GetPrimAtPath(linePath));
    //   auto pointsAttr = curves.GetPointsAttr();
    //   pxr::VtVec3fArray pts;
    //   pointsAttr.Get(&pts, pxr::UsdTimeCode::Default());
    //   pts[1] = pxr::GfVec3f((float)end.x, (float)end.y, (float)end.z);
    //   pointsAttr.Set(pts, pxr::UsdTimeCode::Default());   // overwrite, never accumulate
    //
    //   if (endArrowhead) {
    //       // Re-define (overwrite) the arrowhead mesh prim at endArrowheadPath
    //       // pointing from start (pts[0]) toward the new end.
    //   }
    // Replaces LineNode::setEnd (OsgUtils.cc lines 360-373).
    throw cRuntimeError("UsdLineNode::setEnd(): not implemented yet (milestone M7)");
}

} // namespace usd

} // namespace inet
