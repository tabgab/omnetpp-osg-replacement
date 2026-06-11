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

// NOTE: No pxr:: includes here in M2 — keep the header light.
// The Hydra engine (UsdImagingGLEngine, GfCamera, HdDriver, Hgi) arrives in M3.
// See usdviewer.cc and docs/02-architecture.md §4 for the full M3 plan.

#include "qtenv/iosgviewer.h"

#include <QtWidgets/QToolBar>
#include <QtWidgets/QGridLayout>

namespace omnetpp {

class cOsgCanvas;

namespace qtenv {

/**
 * @brief OpenUSD-backed 3D viewer widget, implementing IOsgViewer via the
 * existing OSG-viewer plugin seam (IOsgViewerFactory / loadExtensionLibrary).
 *
 * Milestone breakdown
 * -------------------
 *  M2 (this file)  — skeleton: all pure-virtual overrides present, bodies are
 *                    no-ops; paintEvent draws a placeholder label.
 *  M3              — Hydra rendering: one UsdImagingGLEngine per UsdViewer,
 *                    sharing a global Hgi/HdDriver (Decision D6).
 *                    IMPORTANT: Qt::AA_ShareOpenGLContexts must be set before
 *                    QApplication is created (Qtenv startup, see docs/02 §4).
 *                    Per-platform Hgi backend (D6):
 *                      • Linux / Windows  → HgiGL  (requires OpenGL ≥ 4.5)
 *                      • macOS            → HgiMetal + hgiInterop to composite
 *                                           Metal output into the QOpenGLWidget FBO
 *                    A 30 fps QBasicTimer heartbeat (own, cannot reuse the OSG
 *                    HeartBeat) gated on !engine->IsConverged() drives repaints.
 *                    engine->SetLightingState(headlight) provides the default
 *                    headlight — Storm has no implicit light (unlike OSG).
 *  M4              — Picking: objectsAt() calls engine->TestIntersection() →
 *                    hitPrimPath → ancestor SdfPath walk (GetParentPath()) through
 *                    the StageCache prim→cObject registry, then Q_EMIT objectsPicked.
 *  M5              — applyViewerHints(): read cOsgCanvas hints (clear colour, FOV,
 *                    camera-manipulator type, z-limits) and push them to the engine.
 *                    Camera controllers: pure-math GfMatrix4d controllers
 *                    (TrackballController, TerrainController, OverviewController).
 *  M6              — Text overlay via QPainter/QImage (USD has no text prim).
 *                    Hydra leaves GL pixel-transfer state that corrupts QPainter;
 *                    use the verified usdview workaround (render text to QImage/
 *                    texture then blit a screen-space quad, or end native rendering
 *                    before the overlay pass). See docs/02 §4.2.
 *
 * Stage conventions (Decision Q8)
 * --------------------------------
 * All stages are authored Z-up with metersPerUnit=1 (matching OSG/INET convention):
 *   UsdGeomSetStageUpAxis(stage, UsdGeomTokens->z)
 *   UsdGeomSetStageMetersPerUnit(stage, 1.0)
 * The asset pipeline (M7) must bake a Y-up→Z-up correction (+90° X-rotation) into
 * converted .osgb → glTF → USD models so they sit correctly in a Z-up metres scene.
 */
class QTENV_API UsdViewer : public IOsgViewer
{
    Q_OBJECT

    // ---------------------------------------------------------------------------
    // Data members
    // ---------------------------------------------------------------------------

    /** The canvas whose scene this viewer is rendering. nullptr until setOsgCanvas(). */
    cOsgCanvas *osgCanvas = nullptr;

    // TODO(M3): UsdImagingGLEngine *engine = nullptr;
    //   One engine per viewer; shared global Hgi/HdDriver — see class doc above.
    //   Construction: engine = new UsdImagingGLEngine(driver); where driver
    //   is created once at Qtenv startup and passed (or stored in a singleton).
    //   Destruction in disable() / destructor.

    // TODO(M3): QBasicTimer heartbeatTimer;
    //   30 fps, started in enable(), stopped in disable(); fires timerEvent()
    //   which calls update() when !engine->IsConverged().

    // TODO(M5): camera-controller object (TrackballController / TerrainController /
    //   OverviewController, selected from osgCanvas->getCameraManipulatorType()).

  public Q_SLOTS:
    /**
     * Re-reads cOsgCanvas viewer hints and pushes them to the engine.
     * Called automatically by setOsgCanvas() and by the inspector toolbar.
     *
     * TODO(M5): implement — push hints to engine:
     *   setClearColor, camera manipulator type, FOV, z-limits.
     *   See OsgViewer::applyViewerHints() for the equivalent OSG logic.
     */
    void applyViewerHints() override;

  public:

    /**
     * Construct the viewer widget.
     * Installs a QGridLayout with 4 px content margins (same as DummyOsgViewer
     * and OsgViewer) so the floating toolbar can be positioned correctly.
     */
    UsdViewer(QWidget *parent = nullptr);

    // ---------------------------------------------------------------------------
    // IOsgViewer pure-virtual overrides
    // ---------------------------------------------------------------------------

    /**
     * Position @p toolbar inside this widget (top-right corner).
     * Mirrors DummyOsgViewer::setFloatingToolbar / OsgViewer::setFloatingToolbar.
     */
    void setFloatingToolbar(QToolBar *toolbar) override;

    /**
     * Set the cOsgCanvas to display.
     * Stores the pointer, calls applyViewerHints(), and schedules a repaint.
     * Passing nullptr clears the canvas.
     *
     * TODO(M3): also trigger StageCache lookup / build for the new canvas.
     * TODO(M5): apply camera manipulator after applyViewerHints().
     */
    void setOsgCanvas(cOsgCanvas *canvas) override;

    /**
     * Return the currently displayed cOsgCanvas, or nullptr.
     */
    cOsgCanvas *getOsgCanvas() const override { return osgCanvas; }

    /**
     * Called when this viewer instance becomes the active view (e.g. the user
     * switches the inspector to 3D mode).
     *
     * TODO(M3): create/associate the Hgi/HdDriver, start the heartbeat timer.
     */
    void enable() override;

    /**
     * Called when the viewer is hidden or another view takes over.
     *
     * TODO(M3): stop the heartbeat timer; release engine resources if not shared.
     */
    void disable() override;

    /**
     * Redraw the scene with the current canvas contents.
     * Skeleton: schedules a Qt repaint via update().
     *
     * TODO(M3): also re-sync the UsdStage from StageCache if the scene changed.
     */
    void refresh() override;

    /**
     * Called when the canvas is cleared (osgCanvas set back to nullptr).
     * Should reset camera / clear colour to neutral defaults.
     *
     * TODO(M5): reset GfCamera to default, clear colour to grey (matches
     *   OsgViewer::resetViewer logic).
     */
    void resetViewer() override;

    /**
     * Return cObject pointers hit at widget coordinate @p pos.
     * Skeleton: always returns the empty set.
     *
     * TODO(M4): call engine->TestIntersection(resolveDeep=true, pos, ...) to
     *   obtain hitPrimPath; walk hitPrimPath.GetParentPath() ancestors through
     *   StageCache's SdfPath→cObject registry to collect matches; Q_EMIT
     *   objectsPicked(objects).  Mirrors OsgViewer::objectsAt() NodePath walk.
     */
    std::vector<cObject *> objectsAt(const QPoint &pos) override;

    // ---------------------------------------------------------------------------
    // Static lifecycle
    // ---------------------------------------------------------------------------

    /**
     * Release any static / global resources held by the USD viewer subsystem.
     * Called from UsdViewerFactory::shutdown() (analogous to OsgViewer::uninit()).
     *
     * TODO(M3): destroy the shared Hgi / HdDriver created at first-viewer-init.
     */
    static void uninit();

  protected:

    /**
     * Placeholder rendering.  Fills the widget with the disabled palette window
     * colour and draws a centred status message — identical pattern to
     * DummyOsgViewer::paintEvent (iosgviewer.cc lines 113-128).
     *
     * TODO(M3): replace with paintGL() (QOpenGLWidget's normal render path) that
     * drives the UsdImagingGLEngine render loop.
     */
    void paintEvent(QPaintEvent *event) override;

  Q_SIGNALS:
    /** Emitted when the user picks objects in the 3D scene (left-click / picking).
     *  Re-declared here for moc; the signal is defined in IOsgViewer but must be
     *  listed in each Q_OBJECT class for correct connection in QT_NO_KEYWORDS mode.
     */
    void objectsPicked(const std::vector<cObject*>&);
};

} // qtenv
} // omnetpp

#endif // __OMNETPP_QTENV_USD_USDVIEWER_H
