// ==========================================================================
//  usd_qt_spike / main.cpp
//
//  De-risking spike: standalone Qt 6 + UsdImagingGLEngine application.
//
//  Validates:
//    R3 — Qt/Hydra GL context interop (HgiGL on Linux, HgiMetal+hgiInterop
//          on macOS); GL-version check; shared Hgi across multiple viewers.
//    R2 — UsdImagingGLEngine::TestIntersection picking, SdfPath surfaced.
//   R11 — WSL2/WSLg note only; no native Windows code needed here.
//   QPainter overlay — two modes: direct QPainter (default) and
//          QImage-blit (--safe-overlay) that survives Hydra GL state leakage.
//
//  Target: OpenUSD >= 25.11, Qt 6, C++17.
//  API surface pinned to OpenUSD 25.11.
// ==========================================================================

// ---------------------------------------------------------------------------
// Qt — using Q_SIGNALS / Q_SLOTS / Q_EMIT throughout because QT_NO_KEYWORDS
// is defined (required: USD headers pull in TBB/Boost that redefine 'signals',
// 'slots', 'emit' if Qt's keyword macros are active).
// ---------------------------------------------------------------------------
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>
#include <QtOpenGLWidgets/QOpenGLWidget>
#include <QtGui/QSurfaceFormat>
#include <QtGui/QOpenGLContext>   // QOpenGLWidget::context() returns QOpenGLContext*
#include <QtGui/QOpenGLFunctions> // raw GL for the R3 present-path bisect diagnostic
#include <QtGui/QPainter>
#include <QtGui/QImage>           // --safe-overlay (QImage-blit text path)
#include <QtGui/QFont>
#include <QtGui/QMouseEvent>
#include <QtCore/QTimer>
#include <QtCore/QDebug>
#include <QtCore/QCommandLineParser>
#include <QtCore/QCommandLineOption>
#include <QtCore/QCoreApplication>

// Standard library
#include <memory>   // std::unique_ptr (s_hgi, w2)
#include <cmath>    // std::cos/sin/atan2/sqrt, M_PI (orbit math)

// ---------------------------------------------------------------------------
// OpenUSD
// ---------------------------------------------------------------------------
// Stage / scene description
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/sphere.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>

// Imaging / Hydra
#include <pxr/usdImaging/usdImagingGL/engine.h>
#include <pxr/usdImaging/usdImagingGL/renderParams.h>

// Hgi (GPU abstraction layer)
#include <pxr/imaging/hgi/hgi.h>
#include <pxr/imaging/hgi/tokens.h>
#include <pxr/imaging/hd/driver.h>

// Lighting
#include <pxr/imaging/glf/simpleLight.h>
#include <pxr/imaging/glf/simpleMaterial.h>

// Camera / framing utilities
#include <pxr/imaging/cameraUtil/framing.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/frustum.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/base/gf/vec2i.h>
// NOTE: there is no GfRange2i in OpenUSD (only range{1,2,3}{d,f} + rect2i) — verified
// against OpenUSD 25.11. CameraUtilFraming is built from a GfRect2i below, so rect2i.h
// is all that's needed here.
#include <pxr/base/gf/rect2i.h>   // GfRect2i (CameraUtilFraming framing rect)
#include <pxr/base/vt/value.h>

// Convenience: bring the pxr namespace into scope for this throwaway spike.
// Production code in the plugin should use fully-qualified names.
PXR_NAMESPACE_USING_DIRECTIVE

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
class SpikeWidget;
static UsdStageRefPtr buildTestStage();

// ---------------------------------------------------------------------------
// Global / shared Hgi state
//
// One Hgi instance is shared across all SpikeWidget instances, mirroring the
// one-Hgi-many-views design planned for the production plugin (docs/02 §4).
// Created lazily inside the first initializeGL() call, when a valid current
// GL context is guaranteed by Qt.
//
// On Linux:  Hgi::CreatePlatformDefaultHgi() → HgiGL  (requires GL >= 4.5)
// On macOS:  Hgi::CreatePlatformDefaultHgi() → HgiMetal (GL capped at 4.1;
//            hgiInterop composites Metal output into the QOpenGLWidget FBO)
// ---------------------------------------------------------------------------
static std::unique_ptr<Hgi>  s_hgi;
static HdDriver              s_driver;   // wraps s_hgi.get() as a VtValue

// Command-line flag: use QImage blit overlay instead of direct QPainter text.
// Set via  --safe-overlay  on the command line.
static bool g_safeOverlay = false;

// ---------------------------------------------------------------------------
// buildTestStage
//
// Creates an in-memory USD stage with:
//   • Z-up, metersPerUnit=1.0 (Decision Q8 — matches OSG/INET conventions)
//   • /World  (UsdGeomXform)
//   • /World/Sphere  — blue sphere, radius 1.0, displayColor (0.2, 0.5, 0.9)
//   • /World/Marker  — small white sphere at (2, 0, 0) — second pick target
// ---------------------------------------------------------------------------
static UsdStageRefPtr buildTestStage()
{
    UsdStageRefPtr stage = UsdStage::CreateInMemory();

    // Coordinate system — must be set before any geometry is authored.
    UsdGeomSetStageUpAxis(stage, UsdGeomTokens->z);      // Z-up (not USD default Y-up)
    UsdGeomSetStageMetersPerUnit(stage, 1.0);            // meters (not USD default cm)

    // World xform prim — root of the scene hierarchy.
    UsdGeomXform world = UsdGeomXform::Define(stage, SdfPath("/World"));
    (void)world;

    // Primary sphere — the main pickable target.
    UsdGeomSphere sphere = UsdGeomSphere::Define(stage, SdfPath("/World/Sphere"));
    sphere.GetRadiusAttr().Set(1.0);

    // displayColor primvar — Storm/HdStorm reads this for unlit/default shading.
    UsdGeomPrimvarsAPI pvAPI(sphere.GetPrim());
    UsdGeomPrimvar colorPv = pvAPI.CreatePrimvar(
        TfToken("displayColor"),
        SdfValueTypeNames->Color3fArray,
        UsdGeomTokens->constant);
    VtArray<GfVec3f> colors = { GfVec3f(0.2f, 0.5f, 0.9f) };
    colorPv.Set(colors);

    // Secondary sphere — offset along X, for two-target pick testing (R2).
    UsdGeomSphere marker = UsdGeomSphere::Define(stage, SdfPath("/World/Marker"));
    marker.GetRadiusAttr().Set(0.3);

    // Place the marker at (2, 0, 0) via a translate xformOp.
    UsdGeomXformable markerXf(marker.GetPrim());
    UsdGeomXformOp translateOp = markerXf.AddTranslateOp();
    translateOp.Set(GfVec3d(2.0, 0.0, 0.0));

    UsdGeomPrimvarsAPI pvAPI2(marker.GetPrim());
    UsdGeomPrimvar markerColor = pvAPI2.CreatePrimvar(
        TfToken("displayColor"),
        SdfValueTypeNames->Color3fArray,
        UsdGeomTokens->constant);
    VtArray<GfVec3f> wc = { GfVec3f(1.0f, 1.0f, 0.8f) };
    markerColor.Set(wc);

    return stage;
}

// ===========================================================================
//  SpikeWidget
//
//  A QOpenGLWidget that renders a UsdStage via UsdImagingGLEngine (Hydra 2 /
//  Storm).  Each widget owns its own engine but shares the global s_hgi.
// ===========================================================================
class SpikeWidget : public QOpenGLWidget
{
    Q_OBJECT   // note: AUTOMOC handles this; moc is driven by CMake AUTOMOC=ON

public:
    explicit SpikeWidget(UsdStageRefPtr stage, const QString &title, QWidget *parent = nullptr)
        : QOpenGLWidget(parent)
        , m_stage(stage)
        , m_title(title)
    {
        setWindowTitle(title);
        resize(800, 600);
    }

    ~SpikeWidget() override
    {
        // Ensure the engine is destroyed while the GL context is current so
        // that Hydra/HgiGL can clean up GPU resources safely.
        makeCurrent();
        m_engine.reset();
        doneCurrent();
    }

protected:
    // -----------------------------------------------------------------------
    // initializeGL
    // -----------------------------------------------------------------------
    void initializeGL() override
    {
        QSurfaceFormat fmt = context()->format();
        qDebug() << m_title
                 << "GL context:" << fmt.majorVersion() << "." << fmt.minorVersion()
                 << "profile:" << fmt.profile()
                 << "renderableType:" << fmt.renderableType();

#ifndef Q_OS_MACOS
        // HgiGL requires OpenGL >= 4.5.  On macOS GL is capped at 4.1, but
        // the macOS path uses HgiMetal, not HgiGL, so this check only applies
        // on non-Apple platforms.
        if (fmt.majorVersion() < 4 || (fmt.majorVersion() == 4 && fmt.minorVersion() < 5)) {
            qFatal("FATAL: OpenGL >= 4.5 is required for HgiGL (Storm renderer). "
                   "Got %d.%d.  Aborting.  "
                   "On macOS, the HgiMetal path is used instead (no GL requirement).",
                   fmt.majorVersion(), fmt.minorVersion());
        }
#endif

        // ------------------------------------------------------------------
        // Create the shared Hgi — only once, on the first widget to initialize.
        // Hgi::CreatePlatformDefaultHgi() selects:
        //   Linux/Windows → HgiGL
        //   macOS         → HgiMetal
        // Both are contained in the monolithic usd_ms library.
        // ------------------------------------------------------------------
        if (!s_hgi) {
            s_hgi = Hgi::CreatePlatformDefaultHgi();
            if (!s_hgi) {
                qFatal("FATAL: Hgi::CreatePlatformDefaultHgi() returned nullptr. "
                       "Check that the USD imaging libraries (usd_ms) are correctly linked.");
            }
            // HdDriver carries the Hgi pointer as a VtValue into the engine.
            // API surface: OpenUSD 25.11.
            s_driver = HdDriver{ HgiTokens->renderDriver, VtValue(s_hgi.get()) };
            qDebug() << "Shared Hgi created:" << QString::fromStdString(s_hgi->GetAPIName());
        }

        // ------------------------------------------------------------------
        // Create this widget's UsdImagingGLEngine.
        //
        // OpenUSD 25.11 API note:
        //   The preferred construction path is UsdImagingGLEngine::Parameters.
        //   If the Parameters struct lacks a 'driver' member in the pinned
        //   release, use the constructor overload:
        //     UsdImagingGLEngine(const HdDriver& driver,
        //                        const TfToken& rendererPluginId = TfToken(),
        //                        bool gpuEnabled = true)
        //   README records which form was actually required.
        // ------------------------------------------------------------------
        UsdImagingGLEngine::Parameters params;
        params.driver = s_driver;
        m_engine = std::make_unique<UsdImagingGLEngine>(params);

        if (!m_engine) {
            qFatal("FATAL: Failed to create UsdImagingGLEngine.");
        }
        qDebug() << m_title << "engine renderer:" << QString::fromStdString(
            m_engine->GetCurrentRendererId().GetString());

        // Start the 33 ms heartbeat that drives on-demand re-renders.
        // The timer calls update() only while the engine has not converged,
        // matching the production plugin's render-loop gating design.
        connect(&m_heartbeat, &QTimer::timeout, this, [this]() {
            if (m_engine && !m_engine->IsConverged())
                update();
        });
        m_heartbeat.start(33);  // ~30 fps
    }

    // -----------------------------------------------------------------------
    // paintGL
    //
    // Per docs/02 §4 sketch.  Order of operations:
    //   1. Inform the engine where to write (the widget's FBO).
    //   2. Set render buffer size (in physical pixels, accounting for HiDPI).
    //   3. Set viewport framing.
    //   4. Set camera (view + projection matrices).
    //   5. Set explicit lighting (Storm has NO implicit headlight).
    //   6. Render.
    //   7. Draw text overlay via QPainter (direct or QImage-blit mode).
    // -----------------------------------------------------------------------
    void paintGL() override
    {
        if (!m_engine || !m_stage)
            return;

        // --- R3 present-path BISECT DIAGNOSTIC ------------------------------
        // Clear the widget's framebuffer to magenta with raw GL *before* Hydra
        // renders. Outcome on screen tells us which layer fails:
        //   • scene visible   -> Metal->GL interop works (problem elsewhere)
        //   • magenta window  -> QOpenGLWidget present works; Hydra's interop is
        //                        NOT compositing into this FBO  (the suspected bug)
        //   • black window     -> GL output never reaches the screen
        //                        (QOpenGLWidget/context issue, not Hydra)
        if (auto *glf = QOpenGLContext::currentContext()->functions()) {
            glf->glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
            glf->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
        // --------------------------------------------------------------------

        const qreal dpr = devicePixelRatioF();
        const int w = width();
        const int h = height();
        const int pw = static_cast<int>(w * dpr);  // physical width
        const int ph = static_cast<int>(h * dpr);  // physical height

        // 1. Direct the engine to render into this widget's FBO.
        //    On macOS, hgiInterop will composite the Metal output into this FBO.
        //    On Linux, HgiGL renders directly into it.
        m_engine->SetPresentationOutput(
            HgiTokens->OpenGL,
            VtValue(static_cast<uint32_t>(defaultFramebufferObject())));

        // 2. Physical render buffer size (HiDPI-aware).
        m_engine->SetRenderBufferSize(GfVec2i(pw, ph));

        // 3. Framing rectangle — maps the render buffer to the display window.
        CameraUtilFraming framing(GfRect2i(GfVec2i(0, 0), pw, ph));
        m_engine->SetFraming(framing);

        // 4. Camera — Z-up convention throughout (Decision Q8).
        GfMatrix4d view;
        view.SetLookAt(m_eye, m_center, m_up);

        GfFrustum frustum;
        frustum.SetPerspective(
            30.0,                         // fovy degrees
            static_cast<double>(w) / h,   // aspect
            0.1,                          // zNear
            1000.0);                      // zFar
        // Store frustum for use in picking (mousePressEvent).
        m_frustum = frustum;
        m_viewMatrix = view;

        GfMatrix4d proj = frustum.ComputeProjectionMatrix();
        m_engine->SetCameraState(view, proj);

        // 5. Explicit lighting — Storm provides NO default headlight.
        //    A single positional light placed at the eye gives a reasonable
        //    Phong shading without the flat unlit look.
        GlfSimpleLight headlight;
        headlight.SetPosition(GfVec4f(
            static_cast<float>(m_eye[0]),
            static_cast<float>(m_eye[1]),
            static_cast<float>(m_eye[2]),
            1.0f));                        // w=1 → positional
        headlight.SetDiffuse(GfVec4f(1.0f, 1.0f, 1.0f, 1.0f));
        headlight.SetSpecular(GfVec4f(1.0f, 1.0f, 1.0f, 1.0f));
        headlight.SetAmbient(GfVec4f(0.0f, 0.0f, 0.0f, 1.0f));

        GlfSimpleMaterial material;   // default: white diffuse/specular, shininess=32
        GfVec4f sceneAmbient(0.1f, 0.1f, 0.1f, 1.0f);
        m_engine->SetLightingState({headlight}, material, sceneAmbient);

        // 6. Render the entire stage.
        UsdImagingGLRenderParams rp;
        rp.clearColor      = GfVec4f(0.5f, 0.5f, 0.86f, 1.0f);  // periwinkle background
        rp.frame           = UsdTimeCode::Default();
        rp.enableLighting  = true;

        m_engine->Render(m_stage->GetPseudoRoot(), rp);

        // [diag] surface any GL error left by the interop/composite step.
        if (auto *glf = QOpenGLContext::currentContext()->functions()) {
            GLenum err = glf->glGetError();
            if (err != 0)
                qWarning() << "[diag] GL error after Render: 0x" + QString::number(err, 16);
            qInfo() << "[diag] presented into FBO" << defaultFramebufferObject()
                    << "converged:" << m_engine->IsConverged();
        }

        // 7. Text overlay.
        //    Hydra/HgiGL can leave GL pixel-transfer state (pack/unpack alignment,
        //    PBO bindings, etc.) that corrupts QPainter's internal raster engine
        //    when it tries to upload glyph bitmaps.
        //
        //    Two modes:
        //      default (direct QPainter): works on most drivers after Hydra 2
        //          scene-index flush — fine on Linux Mesa/NVIDIA in practice.
        //      --safe-overlay (QImage blit): render text into a QImage first,
        //          then drawImage() — no glyph upload from QPainter, so immune
        //          to the GL state leakage.  Needed on some macOS Metal drivers.
        //    README records which mode was required on each platform.
        drawOverlay();
    }

    // -----------------------------------------------------------------------
    // resizeGL
    // -----------------------------------------------------------------------
    void resizeGL(int /*w*/, int /*h*/) override
    {
        // paintGL already reads width()/height() + devicePixelRatioF() each
        // frame, so no extra work is needed here beyond invalidating.
        update();
    }

    // -----------------------------------------------------------------------
    // mousePressEvent — picking via UsdImagingGLEngine::TestIntersection
    //
    // The pick frustum is narrowed to a 1-pixel window around the click,
    // matching the SdfPath→registry pattern planned for the production plugin
    // (docs/02 §5, Decision D4).
    // -----------------------------------------------------------------------
    void mousePressEvent(QMouseEvent *event) override
    {
        if (!m_engine || !m_stage)
            return;

        if (event->button() == Qt::LeftButton) {
            const int w = width();
            const int h = height();

            // Convert click position to NDC [-1, 1] (Y flipped: Qt Y-down → NDC Y-up).
            const double ndcX = (2.0 * event->position().x() / w) - 1.0;
            const double ndcY = 1.0 - (2.0 * event->position().y() / h);

            // Reconstruct the same frustum used in the last paintGL().
            // Transform it to world space by applying the inverse view matrix.
            GfFrustum pickFrustum = m_frustum;
            pickFrustum.Transform(m_viewMatrix.GetInverse());

            // Narrow to a 1-pixel-wide window centred on the click.
            GfFrustum narrowed = pickFrustum.ComputeNarrowedFrustum(
                GfVec2d(ndcX, ndcY),
                GfVec2d(1.0 / w, 1.0 / h));

            GfMatrix4d pickView = narrowed.ComputeViewMatrix();
            GfMatrix4d pickProj = narrowed.ComputeProjectionMatrix();

            GfVec3d    hitPoint;
            GfVec3d    hitNormal;
            SdfPath    hitPrimPath;
            SdfPath    hitInstancerPath;
            int        hitInstanceIndex = 0;

            UsdImagingGLRenderParams rp;
            rp.frame = UsdTimeCode::Default();

            bool hit = m_engine->TestIntersection(
                pickView,
                pickProj,
                m_stage->GetPseudoRoot(),
                rp,
                &hitPoint,
                &hitNormal,
                &hitPrimPath,
                &hitInstancerPath,
                &hitInstanceIndex);

            if (hit) {
                m_hudText = QString("Hit: %1").arg(
                    QString::fromStdString(hitPrimPath.GetString()));
                qDebug() << m_title << "pick hit:" << m_hudText
                         << "at" << hitPoint[0] << hitPoint[1] << hitPoint[2];
            } else {
                m_hudText = "(no hit)";
                qDebug() << m_title << "pick: no hit";
            }
            update();
        }

        // Mouse-drag orbit: record start position.
        if (event->button() == Qt::LeftButton) {
            m_lastMousePos = event->position();
        }
    }

    // -----------------------------------------------------------------------
    // mouseMoveEvent — simple horizontal drag orbits eye around center (+Z up)
    //
    // Proves interactive re-render works.  Rotates the eye vector around the
    // world +Z axis in response to horizontal drag, and tilts it vertically
    // for vertical drag.  (~20 lines as requested.)
    // -----------------------------------------------------------------------
    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!(event->buttons() & Qt::LeftButton))
            return;

        const QPointF delta = event->position() - m_lastMousePos;
        m_lastMousePos = event->position();

        // Horizontal drag → azimuth rotation around +Z.
        const double azimuthDeg  = -delta.x() * 0.5;
        // Vertical drag   → elevation rotation (clamped later).
        const double elevationDeg = delta.y() * 0.5;

        // Vector from center to eye.
        GfVec3d arm = m_eye - m_center;

        // Azimuth: rotate arm around +Z by azimuthDeg.
        const double azRad = azimuthDeg * M_PI / 180.0;
        const double cosA = std::cos(azRad);
        const double sinA = std::sin(azRad);
        GfVec3d rotatedArm(
            cosA * arm[0] - sinA * arm[1],
            sinA * arm[0] + cosA * arm[1],
            arm[2]);

        // Elevation: tilt in the vertical plane containing arm and +Z.
        const double len = rotatedArm.GetLength();
        const double elRad = elevationDeg * M_PI / 180.0;
        const double currentEl = std::atan2(rotatedArm[2],
            std::sqrt(rotatedArm[0]*rotatedArm[0] + rotatedArm[1]*rotatedArm[1]));
        const double newEl = std::max(-M_PI * 0.49, std::min(M_PI * 0.49, currentEl + elRad));

        const double xyLen = len * std::cos(newEl);
        const double az    = std::atan2(rotatedArm[1], rotatedArm[0]);
        m_eye = m_center + GfVec3d(
            xyLen * std::cos(az),
            xyLen * std::sin(az),
            len  * std::sin(newEl));

        update();
    }

private:
    // -----------------------------------------------------------------------
    // drawOverlay — draw HUD text after Hydra rendering.
    // -----------------------------------------------------------------------
    void drawOverlay()
    {
        // Determine backend name for display.
        const QString backend = s_hgi
            ? QString::fromStdString(s_hgi->GetAPIName())
            : QStringLiteral("(no hgi)");

        const QString line1 = m_title + " | " + backend;
        const QString line2 = m_hudText.isEmpty()
            ? QStringLiteral("Left-click to pick")
            : m_hudText;
        const QString line3 = QStringLiteral("Drag to orbit");

        if (g_safeOverlay) {
            // --safe-overlay mode:
            // Render text into a QImage first (no live GL state touched), then
            // blit via drawImage().  This is immune to GL pixel-transfer state
            // left by Hydra/HgiGL that corrupts QPainter glyph uploads.
            // Required on some macOS Metal drivers (see README for findings).
            QImage img(width(), height(), QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::transparent);
            {
                QPainter imgPainter(&img);
                imgPainter.setRenderHint(QPainter::TextAntialiasing);
                QFont f = imgPainter.font();
                f.setPointSize(11);
                imgPainter.setFont(f);
                imgPainter.setPen(Qt::white);
                imgPainter.drawText(10, 20, line1);
                imgPainter.drawText(10, 40, line2);
                imgPainter.drawText(10, 60, line3);
            }
            // Now blit the pre-rendered QImage into the widget via QPainter.
            // drawImage() does not perform glyph uploads, so GL state is safe.
            QPainter blitPainter(this);
            blitPainter.drawImage(0, 0, img);
        } else {
            // Direct QPainter mode (default):
            // Works on most drivers after Hydra 2.  If overlay text appears
            // corrupted or blank, re-run with --safe-overlay.
            QPainter painter(this);
            painter.setRenderHint(QPainter::TextAntialiasing);
            QFont f = painter.font();
            f.setPointSize(11);
            painter.setFont(f);
            painter.setPen(Qt::white);
            painter.drawText(10, 20, line1);
            painter.drawText(10, 40, line2);
            painter.drawText(10, 60, line3);
        }
    }

private:
    UsdStageRefPtr                      m_stage;
    std::unique_ptr<UsdImagingGLEngine> m_engine;

    // Camera state — Z-up convention (matches INET/OSG; Decision Q8).
    GfVec3d  m_eye    { 4.0, 4.0, 3.0 };
    GfVec3d  m_center { 0.0, 0.0, 0.0 };
    GfVec3d  m_up     { 0.0, 0.0, 1.0 };  // Z-up!

    // Stored between paintGL() and mousePressEvent() for picking.
    GfFrustum  m_frustum;
    GfMatrix4d m_viewMatrix;

    QTimer   m_heartbeat;  // 33 ms; fires update() while !IsConverged()
    QString  m_hudText;
    QString  m_title;
    QPointF  m_lastMousePos;
};

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    // -----------------------------------------------------------------------
    // CRITICAL: Qt::AA_ShareOpenGLContexts MUST be set before QApplication.
    //
    // Sharing one Hgi across multiple QOpenGLWidget instances requires that
    // their underlying GL contexts are shared.  Qt creates shared contexts
    // only when this attribute is set before the QApplication is constructed.
    //
    // Qtenv does NOT currently set this (verified: absent in src/qtenv/).
    // M3 must add it in the Qtenv startup path (before QApplication in main.cc
    // or before the first QOpenGLWidget is created).
    // -----------------------------------------------------------------------
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    // -----------------------------------------------------------------------
    // Default surface format — must be set before QApplication too.
    //
    // On macOS:   GL 4.1 CoreProfile (macOS OpenGL ceiling; rendering is
    //             actually done by HgiMetal via hgiInterop, so 4.1 is fine).
    // On Linux:   GL 4.5 CoreProfile (HgiGL requirement).
    //
    // CoreProfile is required because Hydra assumes a core-profile context
    // (no deprecated fixed-function state).
    // -----------------------------------------------------------------------
    {
        QSurfaceFormat fmt;
        fmt.setProfile(QSurfaceFormat::CoreProfile);
        fmt.setDepthBufferSize(24);
        fmt.setStencilBufferSize(8);
#ifdef Q_OS_MACOS
        // macOS caps GL at 4.1.  HgiMetal + hgiInterop handles actual rendering
        // (Decision D6); the QOpenGLWidget context is used only for FBO
        // compositing by hgiInterop, not for issuing draw calls.
        fmt.setVersion(4, 1);
#else
        // Linux/Windows: HgiGL requires GL >= 4.5.
        fmt.setVersion(4, 5);
#endif
        QSurfaceFormat::setDefaultFormat(fmt);
    }

    QApplication app(argc, argv);

    // -----------------------------------------------------------------------
    // Command-line parsing
    // -----------------------------------------------------------------------
    QCommandLineParser parser;
    parser.setApplicationDescription(
        "usd_qt_spike — Qt 6 + UsdImagingGLEngine de-risking spike");
    parser.addHelpOption();

    QCommandLineOption twoOpt("two",
        "Open two SpikeWidget windows sharing the same UsdStage and Hgi.");
    parser.addOption(twoOpt);

    QCommandLineOption safeOverlayOpt("safe-overlay",
        "Use QImage-blit overlay instead of direct QPainter text rendering. "
        "Workaround for Hydra GL pixel-transfer state that corrupts QPainter.");
    parser.addOption(safeOverlayOpt);

    parser.process(app);

    g_safeOverlay = parser.isSet(safeOverlayOpt);

    // -----------------------------------------------------------------------
    // Build the test USD stage (shared across all widgets).
    // -----------------------------------------------------------------------
    UsdStageRefPtr stage = buildTestStage();

    // -----------------------------------------------------------------------
    // Create viewer widget(s).
    //
    // With --two, a second widget is created sharing the SAME stage and the
    // global s_hgi (created inside the first initializeGL()).  This validates:
    //   • AA_ShareOpenGLContexts lets multiple QOpenGLWidgets share contexts.
    //   • One Hgi instance services multiple UsdImagingGLEngine instances.
    // -----------------------------------------------------------------------
    SpikeWidget w1(stage, "SpikeWidget #1");
    w1.show();

    std::unique_ptr<SpikeWidget> w2;
    if (parser.isSet(twoOpt)) {
        w2 = std::make_unique<SpikeWidget>(stage, "SpikeWidget #2");
        w2->show();
    }

    return app.exec();
}

// ---------------------------------------------------------------------------
// MOC inclusion — required when CMAKE_AUTOMOC=ON places the generated moc
// output beside the source file.  Some configurations need an explicit
// include; others generate it automatically.  Including it explicitly here is
// safe and portable across CMake/Qt versions.
// ---------------------------------------------------------------------------
#include "main.moc"
