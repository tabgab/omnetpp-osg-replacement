//==========================================================================
//  USDVIEWER.CC - part of
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

// ---------------------------------------------------------------------------
// How this library hooks into Qtenv
// ---------------------------------------------------------------------------
// This file is compiled into liboppqtenv-usd.so (or .dylib / .dll), which is
// loaded at runtime via:
//
//   loadExtensionLibrary("oppqtenv-usd")
//
// in IOsgViewer::ensureViewerFactory() (iosgviewer.cc).  The global
// usdViewerFactoryInstance object below runs its constructor as soon as the
// dynamic library is mapped into the process, which overwrites the
// osgViewerFactory global (defined in iosgviewer.cc line 28) with a pointer
// to UsdViewerFactory.  This is the same self-registration mechanism used by
// the OSG plugin (RealOsgViewerFactory in osgviewer.cc lines 52-72).
//
// A side-effect of osgViewerFactory being non-null and non-DummyFactory is
// that IOsgViewer::isOsgPreferred() (iosgviewer.cc line 110) returns true,
// so 3D module inspectors open in 3D mode by default.
// ---------------------------------------------------------------------------

#include "usdviewer.h"
#include "stagecache.h"        // M3: StageCache owns UsdStage per cOsgCanvas

#include <QtGui/QPainter>
#include <QtGui/QPalette>
#include <QtWidgets/QGridLayout>

namespace omnetpp {
namespace qtenv {

// ---------------------------------------------------------------------------
// Factory — mirrors RealOsgViewerFactory in osgviewer.cc lines 52-72
// ---------------------------------------------------------------------------

// The global osgViewerFactory pointer lives in iosgviewer.cc; we reference it
// here exactly as osgviewer.cc does (extern declaration, not a redefinition).
extern QTENV_API IOsgViewerFactory *osgViewerFactory;

/**
 * Self-registering factory for UsdViewer.
 *
 * The global usdViewerFactoryInstance object (at the bottom of this file)
 * is constructed when the oppqtenv-usd library is loaded, which sets
 * osgViewerFactory to this instance, replacing the DummyOsgViewerFactory.
 */
class UsdViewerFactory : public IOsgViewerFactory
{
  public:
    UsdViewerFactory()
    {
        // Runs when the library is loaded because of the global
        // usdViewerFactoryInstance object.  Overwrites the dummy factory,
        // making IOsgViewer::isOsgPreferred() return true.
        osgViewerFactory = this;
    }

    IOsgViewer *createViewer() override
    {
        return new UsdViewer();
    }

    void shutdown() override
    {
        UsdViewer::uninit();
    }

    // The USD rendering path never materialises osg::Node objects in the
    // host process, so ref/unref are no-ops here.
    // cOsgCanvas ref-counting for USD scene handles is defined in M4
    // (StageCache owns the UsdStage; it uses a separate ref-count mechanism
    // based on cOsgCanvas pointer identity, not osg::Node intrusive ref-count).
    void refNode(osg::Node *) override {}
    void unrefNode(osg::Node *) override {}
};

// Global instance — construction triggers the self-registration above.
UsdViewerFactory usdViewerFactoryInstance;


// ---------------------------------------------------------------------------
// UsdViewer implementation
// ---------------------------------------------------------------------------

UsdViewer::UsdViewer(QWidget *parent)
    : IOsgViewer(parent)
{
    // Install a QGridLayout so setFloatingToolbar() can position the toolbar.
    // Mirrors DummyOsgViewer constructor (iosgviewer.h lines 105-112) and
    // OsgViewer constructor (osgviewer.cc lines 345-350).
    new QGridLayout(this);

    const int toolbarSpacing = 4; // pixels from the widget edges
    layout()->setContentsMargins(toolbarSpacing, toolbarSpacing,
                                 toolbarSpacing, toolbarSpacing);

    // TODO(M3): create/share the global Hgi and UsdImagingGLEngine here.
    //   Prerequisite: Qt::AA_ShareOpenGLContexts must be set before
    //   QApplication is created (Qtenv startup, see docs/02-architecture §4).
    //   Per-platform Hgi backend (Decision D6):
    //     • Linux / Windows: HgiGL  (OpenGL ≥ 4.5 required)
    //     • macOS:           HgiMetal + hgiInterop composite into QOpenGLWidget FBO
    //   Skeleton: no engine created yet.
}

// ---------------------------------------------------------------------------
// IOsgViewer pure-virtual overrides
// ---------------------------------------------------------------------------

void UsdViewer::setFloatingToolbar(QToolBar *toolbar)
{
    // Mirrors DummyOsgViewer::setFloatingToolbar (iosgviewer.cc line 130-133)
    // and OsgViewer::setFloatingToolbar (osgviewer.cc lines 549-552).
    ((QGridLayout *)layout())->addWidget(toolbar, 0, 0, Qt::AlignRight | Qt::AlignTop);
}

void UsdViewer::setOsgCanvas(cOsgCanvas *canvas)
{
    osgCanvas = canvas;

    // Read hints from the new canvas immediately so the viewer is configured
    // before the first repaint.  applyViewerHints() is a no-op in M2 but will
    // be filled in during M5.
    applyViewerHints();

    // TODO(M3): if canvas != nullptr, look up (or build) the UsdStage via
    //   StageCache::getInstance().getOrCreate(canvas).  If canvas == nullptr,
    //   release the previous stage reference.

    // Schedule a repaint so the placeholder / rendered content is updated.
    update();
}

// NOTE: getOsgCanvas() is defined inline in usdviewer.h.

void UsdViewer::applyViewerHints()
{
    // TODO(M5): read osgCanvas hints and push them to the engine, e.g.:
    //   if (!osgCanvas) return;
    //   cOsgCanvas::Color c = osgCanvas->getClearColor();
    //   params.clearColor = GfVec4f(c.red/255.f, c.green/255.f, c.blue/255.f, 1.f);
    //   setFieldOfView(osgCanvas->getFieldOfViewAngle());
    //   selectCameraController(osgCanvas->getCameraManipulatorType());
    //   if (osgCanvas->hasZLimits()) setZNearFar(osgCanvas->getZNear(), osgCanvas->getZFar());
    //   engine->SetLightingState(headlight); // Storm needs explicit lighting (D2)
    //   update();
    //
    // Mirror: OsgViewer::applyViewerHints() in osgviewer.cc lines 509-531.

    (void)osgCanvas; // suppress unused-variable warning for M2 skeleton
}

void UsdViewer::enable()
{
    // TODO(M3): associate this viewer with the shared CompositeViewer equivalent:
    //   start the per-viewer heartbeat timer (30 fps QBasicTimer, gated on
    //   !engine->IsConverged()).  The OSG HeartBeat singleton cannot be reused
    //   (it drives osgViewer::CompositeViewer::frame()); we need our own.
    //   Mirror: OsgViewer::enable() adding the view to the CompositeViewer
    //   (osgviewer.cc lines 465-471), HeartBeat::start() (osgviewer.cc line 274).
}

void UsdViewer::disable()
{
    // TODO(M3): stop the heartbeat timer.  Release any per-view engine resources
    //   that must not render while invisible.
    //   Mirror: OsgViewer::disable() removing the view from CompositeViewer
    //   (osgviewer.cc lines 473-479), HeartBeat::stop() (osgviewer.cc line 280).
}

void UsdViewer::refresh()
{
    // Skeleton: just schedule a repaint.  The canvas contents are picked up in
    // paintEvent (M2) and will be picked up in paintGL (M3).
    //
    // TODO(M3): re-sync the UsdStage from StageCache if the scene has changed;
    //   call engine->Render(...) in paintGL instead.
    //   Mirror: OsgViewer::refresh() (osgviewer.cc lines 491-507).
    update();
}

void UsdViewer::resetViewer()
{
    // Called when osgCanvas is set to nullptr (viewer cleared).
    // TODO(M5): reset GfCamera to default position/orientation; reset clear
    //   colour to neutral grey (matching OsgViewer::resetViewer colour 0.9/0.9/0.9).
    //   Mirror: OsgViewer::resetViewer() (osgviewer.cc lines 533-541).
}

std::vector<cObject *> UsdViewer::objectsAt(const QPoint &pos)
{
    // TODO(M4): implement USD picking:
    //   1. Call engine->TestIntersection(resolveDeep=true, pos, ...) to obtain
    //      hitPrimPath (SdfPath).
    //   2. Walk hitPrimPath.GetParentPath() ancestors through the StageCache
    //      SdfPath→cObject registry (prim-path → object registry, Decision D4,
    //      docs/02-architecture §5) to collect matching cObject pointers.
    //   3. Q_EMIT objectsPicked(objects) to notify inspectors.
    //   Mirrors OsgViewer::objectsAt() NodePath walk (osgviewer.cc lines 554-575).
    //
    // Note (D4): thin geometry (lines, arrowheads) picks poorly with an ID pass;
    //   use resolveDeep=true plus invisible thick pick-proxy meshes alongside
    //   visible curves where needed (Risk R2).
    (void)pos;
    return {};
}

// ---------------------------------------------------------------------------
// Static lifecycle
// ---------------------------------------------------------------------------

void UsdViewer::uninit()
{
    // TODO(M3): destroy the shared Hgi / HdDriver instance.
    //   The Hgi is a process-wide singleton created at first UsdViewer
    //   construction; it must be destroyed after all UsdImagingGLEngines are gone.
    //   Mirror: OsgViewer::uninit() stopping HeartBeat and releasing the
    //   CompositeViewer (osgviewer.cc lines 543-547).
}

// ---------------------------------------------------------------------------
// Placeholder paintEvent (M2)
// ---------------------------------------------------------------------------

void UsdViewer::paintEvent(QPaintEvent * /*event*/)
{
    // Placeholder rendering — same QPainter-on-QOpenGLWidget pattern as
    // DummyOsgViewer::paintEvent (iosgviewer.cc lines 113-128).
    // This is the same already-working pattern DummyOsgViewer uses, so no
    // special GL-state handling is needed here.
    //
    // TODO(M3): remove this paintEvent override and implement paintGL() to
    //   drive the UsdImagingGLEngine render loop (see docs/02-architecture §4
    //   paintGL() sketch).  Text overlay (M6) goes in a QPainter pass after
    //   Render(); use the verified usdview workaround to avoid GL state
    //   corruption (docs/02 §4.2).

    QPainter painter(this);

    QRect rect({0, 0}, size());

    QPalette pal = palette();
    pal.setCurrentColorGroup(QPalette::Disabled);

    painter.fillRect(rect, pal.window());

    painter.setPen(QPen(pal.text().color()));
    painter.drawText(rect.adjusted(50, 50, -50, -50), // inset so toolbar won't overlap
                     "OpenUSD viewer (skeleton)\n"
                     "Hydra rendering arrives in milestone M3.");
}

} // qtenv
} // omnetpp
