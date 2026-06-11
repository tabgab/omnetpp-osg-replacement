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

#include "usdviewer.h"
#include "usdscenehandle.h"

#include "omnetpp/cosgcanvas.h"
#include "omnetpp/cobject.h"

#include <QtWidgets/QToolBar>
#include <QtWidgets/QGridLayout>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtCore/QDebug>

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usdImaging/usdImagingGL/engine.h>
#include <pxr/usdImaging/usdImagingGL/renderParams.h>
#include <pxr/imaging/hgi/hgi.h>
#include <pxr/imaging/hgi/tokens.h>
#include <pxr/imaging/hd/driver.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/glf/simpleLight.h>
#include <pxr/imaging/glf/simpleMaterial.h>
#include <pxr/imaging/cameraUtil/framing.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/base/gf/vec2i.h>
#include <pxr/base/gf/rect2i.h>
#include <pxr/base/vt/value.h>

#include <cmath>
#include <memory>

PXR_NAMESPACE_USING_DIRECTIVE

namespace omnetpp {
namespace qtenv {

// defined in iosgviewer.cc; the loaded backend library points it at its factory
extern QTENV_API IOsgViewerFactory *osgViewerFactory;

#ifdef Q_OS_MAC
// Metal-native presenter, implemented in usdpresent_metal.mm (spike recipe).
namespace usdmetal {
void *createPresenter(QWidget *host, PXR_NS::Hgi *hgi);
void resizePresenter(void *p, QWidget *host);
void present(void *p, PXR_NS::UsdImagingGLEngine *engine, QWidget *host);
void destroyPresenter(void *p);
}
#endif

// ---------------------------------------------------------------------------
// Shared Hgi/driver — one per process, shared by all UsdViewer instances.
// ---------------------------------------------------------------------------
static std::unique_ptr<Hgi> s_hgi;
static HdDriver s_driver;

static Hgi *ensureSharedHgi()
{
    if (!s_hgi) {
        s_hgi = Hgi::CreatePlatformDefaultHgi();
        if (s_hgi) {
            s_driver = HdDriver{ HgiTokens->renderDriver, VtValue(s_hgi.get()) };
            qInfo() << "UsdViewer: shared Hgi created:"
                    << QString::fromStdString(s_hgi->GetAPIName());
        }
    }
    return s_hgi.get();
}

// ---------------------------------------------------------------------------
// UsdViewer
// ---------------------------------------------------------------------------

UsdViewer::UsdViewer(QWidget *parent) : IOsgViewer(parent)
{
    // same layout convention as DummyOsgViewer / OsgViewer (floating toolbar slot)
    auto *grid = new QGridLayout(this);
    const int toolbarSpacing = 4;
    grid->setContentsMargins(toolbarSpacing, toolbarSpacing, toolbarSpacing, toolbarSpacing);

#ifdef Q_OS_MAC
    // Native child hosting the CAMetalLayer sublayer (validated spike recipe).
    // Transparent for mouse so interaction events land on this viewer.
    metalHost = new QWidget(this);
    metalHost->setAttribute(Qt::WA_NativeWindow);
    metalHost->setAttribute(Qt::WA_NoSystemBackground);
    metalHost->setAttribute(Qt::WA_OpaquePaintEvent);
    metalHost->setAttribute(Qt::WA_TransparentForMouseEvents);
    metalHost->lower();
    metalHost->setGeometry(rect());
    metalHost->show();
#endif

    heartbeat.setInterval(33);  // ~30 fps, render-on-demand
    connect(&heartbeat, &QTimer::timeout, this, [this]() {
        if (engine != nullptr)
            renderNow();
    });
}

UsdViewer::~UsdViewer()
{
#ifdef Q_OS_MAC
    if (presenter) {
        usdmetal::destroyPresenter(presenter);
        presenter = nullptr;
    }
#endif
    delete engine;
    engine = nullptr;
}

cScene3DNode *UsdViewer::sceneHandle() const
{
    return osgCanvas ? osgCanvas->getScene() : nullptr;
}

void UsdViewer::ensureEngine()
{
    if (engine)
        return;
    Hgi *hgi = ensureSharedHgi();
    if (!hgi) {
        qWarning() << "UsdViewer: could not create platform Hgi";
        return;
    }
    UsdImagingGLEngine::Parameters params;
    params.driver = s_driver;
    engine = new UsdImagingGLEngine(params);
#ifdef Q_OS_MAC
    // Metal-native present: we composite the color AOV ourselves.
    engine->SetEnablePresentation(false);
    engine->SetRendererAov(HdAovTokens->color);
#endif
    qInfo() << "UsdViewer: engine renderer:"
            << QString::fromStdString(engine->GetCurrentRendererId().GetString());
}

void UsdViewer::setOsgCanvas(cOsgCanvas *canvas)
{
    if (osgCanvas == canvas)
        return;
    osgCanvas = canvas;
    if (osgCanvas) {
        applyViewerHints();
        ensureEngine();
    }
    refresh();
}

void UsdViewer::applyViewerHints()
{
    if (!osgCanvas)
        return;
    const cOsgCanvas::Viewpoint& vp = osgCanvas->getGenericViewpoint();
    if (vp.valid) {
        camEye    = GfVec3d(vp.eye.x, vp.eye.y, vp.eye.z);
        camCenter = GfVec3d(vp.center.x, vp.center.y, vp.center.z);
        camUp     = GfVec3d(vp.up.x, vp.up.y, vp.up.z);
    }
    refresh();
}

void UsdViewer::setFloatingToolbar(QToolBar *toolbar)
{
    ((QGridLayout *)layout())->addWidget(toolbar, 0, 0, Qt::AlignRight | Qt::AlignTop);
    toolbar->raise();
}

void UsdViewer::enable()
{
    heartbeat.start();
    refresh();
}

void UsdViewer::disable()
{
    heartbeat.stop();
}

void UsdViewer::refresh()
{
#ifdef Q_OS_MAC
    renderNow();
#else
    update();   // schedules paintGL
#endif
}

void UsdViewer::resetViewer()
{
    camEye    = GfVec3d(4.0, 4.0, 3.0);
    camCenter = GfVec3d(0.0, 0.0, 0.0);
    camUp     = GfVec3d(0.0, 0.0, 1.0);
    refresh();
}

void UsdViewer::configureFrame(int pw, int ph)
{
    const int w = std::max(1, width());
    const int h = std::max(1, height());

    GfMatrix4d view;
    view.SetLookAt(camEye, camCenter, camUp);

    double fovy = osgCanvas ? osgCanvas->getFieldOfViewAngle() : 30.0;
    double zNear = (osgCanvas && osgCanvas->hasZLimits()) ? osgCanvas->getZNear() : 0.1;
    double zFar  = (osgCanvas && osgCanvas->hasZLimits()) ? osgCanvas->getZFar()  : 100000.0;

    GfFrustum frustum;
    frustum.SetPerspective(fovy, double(w) / h, zNear, zFar);
    lastFrustum = frustum;
    lastViewMatrix = view;

    engine->SetRenderBufferSize(GfVec2i(pw, ph));
    engine->SetFraming(CameraUtilFraming(GfRect2i(GfVec2i(0, 0), pw, ph)));
    engine->SetCameraState(view, frustum.ComputeProjectionMatrix());

    // Storm has no implicit light: headlight at the eye.
    GlfSimpleLight headlight;
    headlight.SetPosition(GfVec4f(float(camEye[0]), float(camEye[1]), float(camEye[2]), 1.0f));
    headlight.SetDiffuse(GfVec4f(1, 1, 1, 1));
    headlight.SetSpecular(GfVec4f(1, 1, 1, 1));
    headlight.SetAmbient(GfVec4f(0, 0, 0, 1));
    GlfSimpleMaterial material;
    engine->SetLightingState({ headlight }, material, GfVec4f(0.2f, 0.2f, 0.2f, 1.0f));
}

void UsdViewer::renderNow()
{
    static int diagBudget = 8;   // a few release-visible diagnostics, then quiet
    cScene3DNode *handle = sceneHandle();
    if (!handle || !handle->getStage()) {
        if (diagBudget > 0) { --diagBudget;
            qInfo() << "UsdViewer: no scene handle/stage yet (canvas:" << (void*)osgCanvas
                    << "handle:" << (void*)handle << ")"; }
        return;
    }
    ensureEngine();
    if (!engine) {
        if (diagBudget > 0) { --diagBudget; qInfo() << "UsdViewer: no engine"; }
        return;
    }

#ifdef Q_OS_MAC
    if (width() <= 0 || height() <= 0)
        return;
    const qreal dpr = devicePixelRatioF();
    const int pw = std::max(1, int(width() * dpr));
    const int ph = std::max(1, int(height() * dpr));

    configureFrame(pw, ph);

    UsdImagingGLRenderParams rp;
    cOsgCanvas::Color cc = osgCanvas->getClearColor();
    rp.clearColor = GfVec4f(cc.red/255.0f, cc.green/255.0f, cc.blue/255.0f, 1.0f);
    rp.frame = UsdTimeCode::Default();
    rp.enableLighting = true;
    engine->Render(handle->getStage()->GetPseudoRoot(), rp);

    if (!presenter) {
        // pass the shared Hgi directly: engine->GetHgi() returns null for
        // driver-provided Hgis in OpenUSD 25.11
        presenter = usdmetal::createPresenter(metalHost, s_hgi.get());
        qInfo() << "UsdViewer: Metal presenter" << (presenter ? "created" : "FAILED")
                << "host size" << metalHost->width() << "x" << metalHost->height();
    }
    if (presenter)
        usdmetal::present(presenter, engine, metalHost);
#else
    update();   // GL path renders in paintGL
#endif
}

#ifndef Q_OS_MAC
void UsdViewer::initializeGL()
{
    // HgiGL requires OpenGL >= 4.5 (Linux/WSL2 path).
    qDebug() << "UsdViewer: GL context" << context()->format().majorVersion()
             << "." << context()->format().minorVersion();
}

void UsdViewer::paintGL()
{
    cScene3DNode *handle = sceneHandle();
    if (!handle || !handle->getStage())
        return;
    ensureEngine();
    if (!engine)
        return;

    const qreal dpr = devicePixelRatioF();
    const int pw = std::max(1, int(width() * dpr));
    const int ph = std::max(1, int(height() * dpr));

    // HgiGL renders directly; present into this widget's FBO.
    engine->SetEnablePresentation(true);
    engine->SetPresentationOutput(HgiTokens->OpenGL,
        VtValue(uint32_t(defaultFramebufferObject())));

    configureFrame(pw, ph);

    UsdImagingGLRenderParams rp;
    cOsgCanvas::Color cc = osgCanvas->getClearColor();
    rp.clearColor = GfVec4f(cc.red/255.0f, cc.green/255.0f, cc.blue/255.0f, 1.0f);
    rp.frame = UsdTimeCode::Default();
    rp.enableLighting = true;
    engine->Render(handle->getStage()->GetPseudoRoot(), rp);
}
#endif

void UsdViewer::resizeEvent(QResizeEvent *event)
{
    IOsgViewer::resizeEvent(event);
#ifdef Q_OS_MAC
    if (metalHost)
        metalHost->setGeometry(rect());
    if (presenter)
        usdmetal::resizePresenter(presenter, metalHost);
#endif
    refresh();
}

void UsdViewer::showEvent(QShowEvent *event)
{
    IOsgViewer::showEvent(event);
    refresh();
}

// ---------------------------------------------------------------------------
// Interaction: orbit (left-drag), zoom (wheel), pick (left-click)
// ---------------------------------------------------------------------------

std::vector<cObject *> UsdViewer::objectsAt(const QPoint& pos)
{
    std::vector<cObject *> objects;
    cScene3DNode *handle = sceneHandle();
    if (!handle || !handle->getStage() || !engine)
        return objects;

    const int w = std::max(1, width());
    const int h = std::max(1, height());
    const double ndcX = (2.0 * pos.x() / w) - 1.0;
    const double ndcY = 1.0 - (2.0 * pos.y() / h);

    GfFrustum pickFrustum = lastFrustum;
    pickFrustum.Transform(lastViewMatrix.GetInverse());
    // ~6px pick window: a 1px window makes picking feel unreliable with the
    // ID-pass-based TestIntersection (Risk R2)
    GfFrustum narrowed = pickFrustum.ComputeNarrowedFrustum(
        GfVec2d(ndcX, ndcY), GfVec2d(6.0 / w, 6.0 / h));

    GfVec3d hitPoint, hitNormal;
    SdfPath hitPrimPath, hitInstancerPath;
    int hitInstanceIndex = 0;
    UsdImagingGLRenderParams rp;
    rp.frame = UsdTimeCode::Default();

    bool hit = engine->TestIntersection(
        narrowed.ComputeViewMatrix(), narrowed.ComputeProjectionMatrix(),
        handle->getStage()->GetPseudoRoot(), rp,
        &hitPoint, &hitNormal, &hitPrimPath, &hitInstancerPath, &hitInstanceIndex);

    if (hit) {
        if (cObject *obj = handle->lookupObjectOrAncestor(hitPrimPath))
            objects.push_back(obj);
    }
    return objects;
}

void UsdViewer::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        lastMousePos = event->position();
        std::vector<cObject *> objects = objectsAt(event->position().toPoint());
        if (!objects.empty())
            Q_EMIT objectsPicked(objects);
    }
}

void UsdViewer::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton))
        return;

    const QPointF delta = event->position() - lastMousePos;
    lastMousePos = event->position();

    const double azimuthDeg   = -delta.x() * 0.5;
    const double elevationDeg =  delta.y() * 0.5;

    GfVec3d arm = camEye - camCenter;
    const double azRad = azimuthDeg * M_PI / 180.0;
    const double cosA = std::cos(azRad), sinA = std::sin(azRad);
    GfVec3d rotatedArm(cosA*arm[0] - sinA*arm[1], sinA*arm[0] + cosA*arm[1], arm[2]);

    const double len = rotatedArm.GetLength();
    const double elRad = elevationDeg * M_PI / 180.0;
    const double currentEl = std::atan2(rotatedArm[2],
        std::sqrt(rotatedArm[0]*rotatedArm[0] + rotatedArm[1]*rotatedArm[1]));
    const double newEl = std::max(-M_PI*0.49, std::min(M_PI*0.49, currentEl + elRad));
    const double xyLen = len * std::cos(newEl);
    const double az = std::atan2(rotatedArm[1], rotatedArm[0]);
    camEye = camCenter + GfVec3d(xyLen*std::cos(az), xyLen*std::sin(az), len*std::sin(newEl));

    renderNow();
}

void UsdViewer::wheelEvent(QWheelEvent *event)
{
    const double steps = event->angleDelta().y() / 120.0;
    const double factor = std::pow(0.9, steps);   // wheel up zooms in
    GfVec3d arm = camEye - camCenter;
    if (arm.GetLength() * factor > 0.01)
        camEye = camCenter + arm * factor;
    renderNow();
}

void UsdViewer::shutdownUsd()
{
    s_hgi.reset();
}

// ---------------------------------------------------------------------------
// Factory — self-registers on library load, like RealOsgViewerFactory.
// ---------------------------------------------------------------------------

class UsdViewerFactory : public IOsgViewerFactory
{
  public:
    UsdViewerFactory() { osgViewerFactory = this; }

    IOsgViewer *createViewer() override { return new UsdViewer(); }
    void shutdown() override { UsdViewer::shutdownUsd(); }

    // The pointers flowing through here are cScene3DNode* in disguise:
    // cEnvir::refSceneNode() reinterpret_casts to osg::Node* for the legacy
    // chain (Qtenv::refOsgNode -> IOsgViewer::refNode -> here); we cast back.
    void refNode(osg::Node *node) override {
        if (node) reinterpret_cast<cScene3DNode *>(node)->ref();
    }
    void unrefNode(osg::Node *node) override {
        if (node) reinterpret_cast<cScene3DNode *>(node)->unref();
    }
};

static UsdViewerFactory usdViewerFactoryInstance;

}  // namespace qtenv
}  // namespace omnetpp
