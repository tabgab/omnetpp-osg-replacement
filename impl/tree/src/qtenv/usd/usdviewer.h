//==========================================================================
//  USDVIEWER.H - part of
//
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

#ifndef __OMNETPP_QTENV_USD_USDVIEWER_H
#define __OMNETPP_QTENV_USD_USDVIEWER_H

#include "qtenv/iosgviewer.h"

#include <QtCore/QTimer>
#include <QtCore/QPointF>

// Keep this header light for moc: only pxr math/value headers + forward decls.
#include <pxr/pxr.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/frustum.h>

PXR_NAMESPACE_OPEN_SCOPE
class UsdImagingGLEngine;
PXR_NAMESPACE_CLOSE_SCOPE

class QToolBar;

namespace omnetpp {

class cScene3DNode;

namespace qtenv {

/**
 * @brief OpenUSD/Hydra-backed implementation of IOsgViewer.
 *
 * Renders the UsdStage carried by the cOsgCanvas's cScene3DNode handle with
 * Hydra Storm. Presentation is per-platform (Decision D6):
 *   - macOS: Metal-native — HgiMetal renders to the color AOV, which is
 *     blitted into a CAMetalLayer hosted on a child widget (the validated
 *     spike recipe; GL is unusable: HgiGL needs >=4.5, macOS caps at 4.1,
 *     and USD's Metal->GL interop does not present into Qt surfaces).
 *   - other platforms (Linux/WSL): HgiGL renders directly into this
 *     QOpenGLWidget via the engine's own GL presentation (paintGL).
 */
class UsdViewer : public IOsgViewer
{
    Q_OBJECT

  public Q_SLOTS:
    void applyViewerHints() override;

  public:
    UsdViewer(QWidget *parent = nullptr);
    ~UsdViewer() override;

    void setFloatingToolbar(QToolBar *toolbar) override;
    void setOsgCanvas(cOsgCanvas *canvas) override;
    cOsgCanvas *getOsgCanvas() const override { return osgCanvas; }

    void enable() override;
    void disable() override;

    void refresh() override;
    void resetViewer() override;

    std::vector<cObject *> objectsAt(const QPoint& pos) override;

    static void shutdownUsd();   // factory shutdown hook

  protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
#ifndef Q_OS_MAC
    void initializeGL() override;
    void paintGL() override;
#endif

  private:
    void ensureEngine();
    cScene3DNode *sceneHandle() const;
    void configureFrame(int pw, int ph);    // camera, lighting, framing
    void renderNow();                       // platform-appropriate render+present

    cOsgCanvas *osgCanvas = nullptr;
    PXR_NS::UsdImagingGLEngine *engine = nullptr;

    // Camera state - Z-up convention (Decision Q8).
    PXR_NS::GfVec3d camEye    { 4.0, 4.0, 3.0 };
    PXR_NS::GfVec3d camCenter { 0.0, 0.0, 0.0 };
    PXR_NS::GfVec3d camUp     { 0.0, 0.0, 1.0 };
    PXR_NS::GfFrustum  lastFrustum;
    PXR_NS::GfMatrix4d lastViewMatrix;

    QTimer heartbeat;          // 30 fps, gated on !IsConverged
    QPointF lastMousePos;

#ifdef Q_OS_MAC
    QWidget *metalHost = nullptr;   // native child hosting the CAMetalLayer
    void *presenter = nullptr;      // opaque, implemented in usdpresent_metal.mm
#endif

  Q_SIGNALS:
    void objectsPicked(const std::vector<cObject*>&);
};

}  // namespace qtenv
}  // namespace omnetpp

#endif
