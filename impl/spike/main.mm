// ==========================================================================
//  usd_qt_spike / main.mm   (Objective-C++)
//
//  De-risking spike: Qt 6 + OpenUSD Hydra (Storm), MACOS METAL-NATIVE present.
//
//  Windowing approach (after two QWindow::MetalSurface crashes in Qt's
//  -[QNSView(Drawing) displayLayer:]): we do NOT use QWindow::MetalSurface.
//  Instead a normal QWidget is forced native, and our own CAMetalLayer is added
//  as a SUBLAYER of the widget's backing layer.  Qt's QNSView keeps its normal
//  (raster) path and never enters the Metal displayLayer: machinery that
//  crashed; our Metal sublayer composites on top; mouse events still reach the
//  QWidget (a CALayer does not intercept events).
//
//  Rendering: HgiMetal + Storm render to a color AOV (id<MTLTexture>), which we
//  blit into the CAMetalLayer drawable with a tiny MSL shader.  No OpenGL, no
//  hgiInterop.  This is the path the real Qtenv viewer will take on macOS
//  (Decision D6 / Risk R3).
//
//  Target: OpenUSD >= 25.11, Qt 6, C++17, macOS (Apple Silicon/Intel).
// ==========================================================================

#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>
#include <QtGui/QMouseEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QShowEvent>
#include <QtGui/QResizeEvent>
#include <QtCore/QTimer>
#include <QtCore/QDebug>
#include <QtCore/QCommandLineParser>
#include <QtCore/QCommandLineOption>
#include <QtCore/QCoreApplication>

#include <memory>
#include <cmath>
#include <cstring>   // memcmp (AOV readback diag)
#include <vector>    // drawable readback diag
#include <cstdio>    // PPM dump ('d' key)
#include <pxr/base/gf/half.h>   // GfHalf — float16 AOV → 8-bit PPM

// ---- OpenUSD ----
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/sphere.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usdImaging/usdImagingGL/engine.h>
#include <pxr/usdImaging/usdImagingGL/renderParams.h>
#include <pxr/imaging/hgi/hgi.h>
#include <pxr/imaging/hgi/tokens.h>
#include <pxr/imaging/hgi/texture.h>
#include <pxr/imaging/hgiMetal/hgi.h>
#include <pxr/imaging/hd/aov.h>
#include <pxr/imaging/hd/renderBuffer.h>
#include <pxr/imaging/hd/types.h>   // HdFormat / HdDataSizeOfFormat (AOV readback diag)
#include <pxr/imaging/glf/simpleLight.h>
#include <pxr/imaging/glf/simpleMaterial.h>
#include <pxr/imaging/cameraUtil/framing.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/frustum.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/base/gf/vec2i.h>
#include <pxr/base/gf/rect2i.h>
#include <pxr/base/vt/value.h>

// ---- Metal / Cocoa ----
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <AppKit/AppKit.h>

PXR_NAMESPACE_USING_DIRECTIVE

static std::unique_ptr<Hgi> s_hgi;
static HdDriver             s_driver;

// ---------------------------------------------------------------------------
// buildTestStage — in-memory Z-up, meters scene (Decision Q8).
// ---------------------------------------------------------------------------
static UsdStageRefPtr buildTestStage()
{
    UsdStageRefPtr stage = UsdStage::CreateInMemory();
    UsdGeomSetStageUpAxis(stage, UsdGeomTokens->z);
    UsdGeomSetStageMetersPerUnit(stage, 1.0);

    UsdGeomXform::Define(stage, SdfPath("/World"));

    UsdGeomSphere sphere = UsdGeomSphere::Define(stage, SdfPath("/World/Sphere"));
    sphere.GetRadiusAttr().Set(1.0);
    {
        UsdGeomPrimvarsAPI pv(sphere.GetPrim());
        UsdGeomPrimvar c = pv.CreatePrimvar(TfToken("displayColor"),
            SdfValueTypeNames->Color3fArray, UsdGeomTokens->constant);
        c.Set(VtArray<GfVec3f>{ GfVec3f(0.2f, 0.5f, 0.9f) });
    }

    UsdGeomSphere marker = UsdGeomSphere::Define(stage, SdfPath("/World/Marker"));
    marker.GetRadiusAttr().Set(0.3);
    UsdGeomXformable(marker.GetPrim()).AddTranslateOp().Set(GfVec3d(2.0, 0.0, 0.0));
    {
        UsdGeomPrimvarsAPI pv(marker.GetPrim());
        UsdGeomPrimvar c = pv.CreatePrimvar(TfToken("displayColor"),
            SdfValueTypeNames->Color3fArray, UsdGeomTokens->constant);
        c.Set(VtArray<GfVec3f>{ GfVec3f(1.0f, 1.0f, 0.8f) });
    }
    return stage;
}

// MSL present shader: fullscreen triangle sampling the Hydra color AOV.
static const char *kPresentMSL = R"MSL(
#include <metal_stdlib>
using namespace metal;
struct VOut { float4 pos [[position]]; float2 uv; };
vertex VOut vmain(uint vid [[vertex_id]]) {
    float2 p = float2((vid << 1) & 2, vid & 2);
    VOut o;
    o.pos = float4(p * 2.0 - 1.0, 0.0, 1.0);
    o.uv  = float2(p.x, 1.0 - p.y);          // flip V (AOV is bottom-up)
    return o;
}
fragment float4 fmain(VOut in [[stage_in]], texture2d<float> tex [[texture(0)]]) {
    constexpr sampler s(coord::normalized, filter::linear, address::clamp_to_edge);
    return tex.sample(s, in.uv);
}
)MSL";

// ===========================================================================
//  SpikeView — a native QWidget hosting a CAMetalLayer sublayer.
// ===========================================================================
class SpikeView : public QWidget
{
    Q_OBJECT
public:
    SpikeView(UsdStageRefPtr stage, const QString &title)
        : m_stage(stage), m_title(title)
    {
        setWindowTitle(title);
        resize(800, 600);
        setAttribute(Qt::WA_NativeWindow);      // ensure our own NSView
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_OpaquePaintEvent);
        connect(&m_heartbeat, &QTimer::timeout, this, [this]() {
            if (m_engine && !m_engine->IsConverged())
                renderNow();
        });
        m_heartbeat.setInterval(33);
    }

    ~SpikeView() override { m_engine.reset(); }

protected:
    void showEvent(QShowEvent *) override { ensureInit(); renderNow(); }
    void resizeEvent(QResizeEvent *) override { updateLayerGeometry(); renderNow(); }
    QPaintEngine *paintEngine() const override { return nullptr; } // we own drawing via Metal

    void mousePressEvent(QMouseEvent *event) override
    {
        if (!m_engine || !m_stage) return;
        if (event->button() == Qt::LeftButton) {
            const int w = width(), h = height();
            const double ndcX = (2.0 * event->position().x() / w) - 1.0;
            const double ndcY = 1.0 - (2.0 * event->position().y() / h);

            GfFrustum pickFrustum = m_frustum;
            pickFrustum.Transform(m_viewMatrix.GetInverse());
            GfFrustum narrowed = pickFrustum.ComputeNarrowedFrustum(
                GfVec2d(ndcX, ndcY), GfVec2d(1.0 / w, 1.0 / h));
            GfMatrix4d pickView = narrowed.ComputeViewMatrix();
            GfMatrix4d pickProj = narrowed.ComputeProjectionMatrix();

            GfVec3d hitPoint, hitNormal;
            SdfPath hitPrimPath, hitInstancerPath;
            int     hitInstanceIndex = 0;
            UsdImagingGLRenderParams rp;
            rp.frame = UsdTimeCode::Default();

            bool hit = m_engine->TestIntersection(
                pickView, pickProj, m_stage->GetPseudoRoot(), rp,
                &hitPoint, &hitNormal, &hitPrimPath, &hitInstancerPath, &hitInstanceIndex);

            if (hit) {
                m_hudText = QString("Hit: %1").arg(QString::fromStdString(hitPrimPath.GetString()));
                qDebug() << m_title << "pick hit:" << m_hudText
                         << "at" << hitPoint[0] << hitPoint[1] << hitPoint[2];
            } else {
                m_hudText = "(no hit)";
                qDebug() << m_title << "pick: no hit";
            }
            setWindowTitle(m_title + " | Metal | " + m_hudText);
            m_lastMousePos = event->position();
        }
    }

    // 'd' dumps the AOV + presented drawable to /tmp/spike_{aov,drawable}.ppm
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_D) {
            m_dumpRequested = true;
            renderNow();
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!(event->buttons() & Qt::LeftButton)) return;
        const QPointF delta = event->position() - m_lastMousePos;
        m_lastMousePos = event->position();

        const double azimuthDeg   = -delta.x() * 0.5;
        const double elevationDeg =  delta.y() * 0.5;
        GfVec3d arm = m_eye - m_center;
        const double azRad = azimuthDeg * M_PI / 180.0;
        const double cosA = std::cos(azRad), sinA = std::sin(azRad);
        GfVec3d rotatedArm(cosA*arm[0]-sinA*arm[1], sinA*arm[0]+cosA*arm[1], arm[2]);
        const double len = rotatedArm.GetLength();
        const double elRad = elevationDeg * M_PI / 180.0;
        const double currentEl = std::atan2(rotatedArm[2],
            std::sqrt(rotatedArm[0]*rotatedArm[0] + rotatedArm[1]*rotatedArm[1]));
        const double newEl = std::max(-M_PI*0.49, std::min(M_PI*0.49, currentEl + elRad));
        const double xyLen = len * std::cos(newEl);
        const double az = std::atan2(rotatedArm[1], rotatedArm[0]);
        m_eye = m_center + GfVec3d(xyLen*std::cos(az), xyLen*std::sin(az), len*std::sin(newEl));
        renderNow();
    }

private:
    void ensureInit()
    {
        if (m_engine) return;

        if (!s_hgi) {
            s_hgi = Hgi::CreatePlatformDefaultHgi();
            if (!s_hgi) qFatal("Hgi::CreatePlatformDefaultHgi() returned nullptr.");
            s_driver = HdDriver{ HgiTokens->renderDriver, VtValue(s_hgi.get()) };
            qDebug() << "Shared Hgi created:" << QString::fromStdString(s_hgi->GetAPIName());
        }

        UsdImagingGLEngine::Parameters params;
        params.driver = s_driver;
        m_engine = std::make_unique<UsdImagingGLEngine>(params);
        m_engine->SetEnablePresentation(false);
        // Canonical recipe (frameRecorder::Record, frameRecorder.cpp): a single
        // color AOV.  The task controller adds depth internally.  (An explicit
        // {color,depth} set, or no call at all, leaves GetAovTexture(color)
        // returning null in 25.11.)
        m_engine->SetRendererAov(HdAovTokens->color);
        qDebug() << m_title << "engine renderer:"
                 << QString::fromStdString(m_engine->GetCurrentRendererId().GetString());

        HgiMetal *hgiMetal = static_cast<HgiMetal*>(s_hgi.get());
        m_device = hgiMetal->GetPrimaryDevice();
        m_queue  = hgiMetal->GetQueue();

        // Add our CAMetalLayer as a SUBLAYER of the widget's backing layer.
        // We never touch/replace the QNSView's own layer (that crashed before).
        NSView *view = (__bridge NSView *)reinterpret_cast<void *>(winId());
        view.wantsLayer = YES;
        m_layer = [CAMetalLayer layer];
        m_layer.device          = m_device;
        m_layer.pixelFormat     = MTLPixelFormatBGRA8Unorm;
        m_layer.framebufferOnly = NO;   // diag: allow getBytes readback of the drawable
        if (view.layer)
            [view.layer addSublayer:m_layer];
        else
            view.layer = m_layer, view.wantsLayer = YES;  // unlikely fallback

        buildPipeline();
        updateLayerGeometry();
        m_heartbeat.start();
    }

    void buildPipeline()
    {
        NSError *err = nil;
        id<MTLLibrary> lib = [m_device newLibraryWithSource:@(kPresentMSL) options:nil error:&err];
        if (!lib) qFatal("present MSL compile failed: %s", err.localizedDescription.UTF8String);
        MTLRenderPipelineDescriptor *d = [[MTLRenderPipelineDescriptor alloc] init];
        d.vertexFunction   = [lib newFunctionWithName:@"vmain"];
        d.fragmentFunction = [lib newFunctionWithName:@"fmain"];
        d.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        m_pipeline = [m_device newRenderPipelineStateWithDescriptor:d error:&err];
        if (!m_pipeline) qFatal("present pipeline failed: %s", err.localizedDescription.UTF8String);
    }

    void updateLayerGeometry()
    {
        if (!m_layer) return;
        const qreal dpr = devicePixelRatioF();
        m_layer.frame         = CGRectMake(0, 0, width(), height());
        m_layer.contentsScale = dpr;
        m_pw = std::max(1, int(width()  * dpr));
        m_ph = std::max(1, int(height() * dpr));
        m_layer.drawableSize  = CGSizeMake(m_pw, m_ph);
    }

    void renderNow()
    {
        ensureInit();
        if (m_pw <= 0 || m_ph <= 0) return;

        GfMatrix4d view; view.SetLookAt(m_eye, m_center, m_up);
        GfFrustum frustum;
        frustum.SetPerspective(30.0, double(width()) / double(height()), 0.1, 1000.0);
        m_frustum = frustum;
        m_viewMatrix = view;

        m_engine->SetRenderBufferSize(GfVec2i(m_pw, m_ph));
        m_engine->SetFraming(CameraUtilFraming(GfRect2i(GfVec2i(0, 0), m_pw, m_ph)));
        m_engine->SetCameraState(view, frustum.ComputeProjectionMatrix());

        GlfSimpleLight light;
        light.SetPosition(GfVec4f(float(m_eye[0]), float(m_eye[1]), float(m_eye[2]), 1.0f));
        light.SetDiffuse(GfVec4f(1, 1, 1, 1));
        light.SetSpecular(GfVec4f(1, 1, 1, 1));
        light.SetAmbient(GfVec4f(0, 0, 0, 1));
        GlfSimpleMaterial material;
        m_engine->SetLightingState({ light }, material, GfVec4f(0.1f, 0.1f, 0.1f, 1.0f));

        UsdImagingGLRenderParams rp;
        rp.clearColor     = GfVec4f(0.5f, 0.5f, 0.86f, 1.0f);
        rp.frame          = UsdTimeCode::Default();
        rp.enableLighting = true;
        m_engine->Render(m_stage->GetPseudoRoot(), rp);

        // CRITICAL ordering fix: when Render() returns, Hydra's Metal work is
        // still sitting in HgiMetal's *uncommitted primary command buffer*.
        // If we commit our present buffer now, Metal executes it FIRST and we
        // sample the AOV before the geometry pass ran (→ clear colour only).
        // hgiInterop does exactly this commit before its GL blit (metal.mm:874).
        HgiMetal *hgiMetal = static_cast<HgiMetal*>(s_hgi.get());
        hgiMetal->CommitPrimaryCommandBuffer(
            HgiMetal::CommitCommandBuffer_WaitUntilCompleted);

        HdRenderBuffer *colorRb = m_engine->GetAovRenderBuffer(HdAovTokens->color);
        if (colorRb)
            colorRb->Resolve();

        // SYNCHRONIZING READBACK — empirically REQUIRED, do not remove.
        // Map() performs a GPU->CPU readback of the color AOV through
        // HgiTextureReadback (blit + SubmitCmds(WaitUntilCompleted)).  Across
        // every instrumented frame (16/16 in two runs) the presented drawable
        // was byte-identical to the AOV when this ran before the present, and
        // went stale (clear-only) within seconds of continuous re-renders when
        // it didn't — even with CommitPrimaryCommandBuffer(WaitUntilCompleted)
        // and a post-present waitUntilCompleted in place.  Whatever ordering
        // HgiTextureReadback establishes inside HgiMetal is load-bearing.
        // Cost ≈ a few ms at 1600x1200 on unified memory — acceptable for the
        // spike.  TODO(M3): replace with a proper fence (MTLSharedEvent) or
        // reuse HgiInterop's commit dance instead of a full readback.
        if (colorRb) {
            if (colorRb->Map())
                colorRb->Unmap();
        }

        // [diag] CPU readback of the color AOV: counts pixels whose raw bytes
        // differ from pixel(0,0).  Format-agnostic flat-vs-geometry bisect:
        //   0 differing  -> the main render pass wrote only the clear colour
        //   >0 differing -> the AOV has geometry and the present path is at fault
        if (m_diagFrames < 4 && colorRb) {
            const int rw = int(colorRb->GetWidth());
            const int rh = int(colorRb->GetHeight());
            const size_t px = HdDataSizeOfFormat(colorRb->GetFormat());
            if (void *data = colorRb->Map()) {
                const unsigned char *p = static_cast<const unsigned char*>(data);
                size_t differing = 0;
                const size_t n = size_t(rw) * size_t(rh);
                for (size_t i = 1; i < n; ++i)
                    if (memcmp(p + i*px, p, px) != 0) ++differing;
                colorRb->Unmap();
                qInfo() << "[diag] AOV readback" << rw << "x" << rh
                        << "fmt" << int(colorRb->GetFormat())
                        << "bytes/px" << int(px)
                        << "pixels differing from (0,0):" << qulonglong(differing);
            } else {
                qWarning() << "[diag] AOV Map() failed";
            }
        }
        // Present the RENDER BUFFER's own (resolved, single-sample) texture —
        // the exact object the CPU readback above proved contains the image.
        // GetAovTexture(color) returns the task-context 'color' texture, which
        // the color-correction/AOV-input tasks ping-pong with 'colorIntermediate';
        // presenting that showed stale/clear content after picking.
        HgiTextureHandle colorH;
        if (colorRb) {
            VtValue res = colorRb->GetResource(/*multiSampled=*/false);
            if (res.IsHolding<HgiTextureHandle>())
                colorH = res.UncheckedGet<HgiTextureHandle>();
        }
        if (!colorH)
            colorH = m_engine->GetAovTexture(HdAovTokens->color);   // fallback
        id<MTLTexture> srcTex = nil;
        if (colorH)
            srcTex = (__bridge id<MTLTexture>)reinterpret_cast<void *>(colorH->GetRawResource());
        if (!srcTex) { qWarning() << "[diag] no color AOV texture"; return; }

        id<CAMetalDrawable> drawable = [m_layer nextDrawable];
        if (!drawable) {
            qWarning() << "[diag] nextDrawable returned nil (layer detached/resized?)";
            return;
        }
        id<MTLCommandBuffer> cb = [m_queue commandBuffer];
        MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture     = drawable.texture;
        pass.colorAttachments[0].loadAction  = MTLLoadActionClear;
        pass.colorAttachments[0].clearColor  = MTLClearColorMake(0, 0, 0, 1);
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        id<MTLRenderCommandEncoder> enc = [cb renderCommandEncoderWithDescriptor:pass];
        [enc setRenderPipelineState:m_pipeline];
        [enc setFragmentTexture:srcTex atIndex:0];
        [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [enc endEncoding];
        [cb presentDrawable:drawable];
        [cb commit];
        // Frame pacing: wait for the present blit to complete before encoding
        // the next frame.  Empirically REQUIRED: without this per-frame sync the
        // present goes stale within seconds of continuous redraws (verified —
        // with it, 12/12 frames had drawable content byte-identical to the AOV).
        // The production viewer (M3) should replace the CPU wait with
        // addCompletedHandler-driven pacing or an MTLSharedEvent, but a
        // synchronous frame is acceptable here (the OSG viewer was effectively
        // synchronous per frame as well).
        [cb waitUntilCompleted];

        ++m_frameCount;

        // Periodic health log (every 30th frame): camera eye + AOV content count.
        // Catches orbit-math blowups (NaN eye) and AOV staleness during drags.
        if (m_frameCount % 30 == 1 && colorRb) {
            size_t aovDiff = 0;
            const size_t px = HdDataSizeOfFormat(colorRb->GetFormat());
            if (void *data = colorRb->Map()) {
                const unsigned char *p = static_cast<const unsigned char*>(data);
                for (size_t i = 1, n = size_t(colorRb->GetWidth())*colorRb->GetHeight(); i < n; ++i)
                    if (memcmp(p + i*px, p, px) != 0) ++aovDiff;
                colorRb->Unmap();
            }
            qInfo() << "[diag] frame" << m_frameCount
                    << "eye(" << m_eye[0] << m_eye[1] << m_eye[2] << ")"
                    << "aovDiff:" << qulonglong(aovDiff)
                    << "layerAttached:" << (m_layer.superlayer != nil);
        }

        // 'd' keypress: dump AOV + presented drawable as PPMs for offline review.
        if (m_dumpRequested) {
            m_dumpRequested = false;
            // AOV (float16 RGBA → 8-bit RGB)
            if (colorRb) {
                const int rw = int(colorRb->GetWidth()), rh = int(colorRb->GetHeight());
                if (void *data = colorRb->Map()) {
                    const uint16_t *hp = static_cast<const uint16_t*>(data);
                    FILE *f = fopen("/tmp/spike_aov.ppm", "wb");
                    fprintf(f, "P6\n%d %d\n255\n", rw, rh);
                    for (size_t i = 0, n = size_t(rw)*rh; i < n; ++i) {
                        for (int c = 0; c < 3; ++c) {
                            GfHalf h; h.setBits(hp[i*4+c]);
                            float v = std::min(1.0f, std::max(0.0f, float(h)));
                            fputc(int(v*255.0f + 0.5f), f);
                        }
                    }
                    fclose(f);
                    colorRb->Unmap();
                }
            }
            // Presented drawable (BGRA8 → RGB)
            {
                id<MTLTexture> dt = drawable.texture;
                const int dw = int(dt.width), dh = int(dt.height);
                std::vector<unsigned char> buf(size_t(dw)*dh*4);
                [dt getBytes:buf.data() bytesPerRow:(NSUInteger)dw*4
                    fromRegion:MTLRegionMake2D(0, 0, dw, dh) mipmapLevel:0];
                FILE *f = fopen("/tmp/spike_drawable.ppm", "wb");
                fprintf(f, "P6\n%d %d\n255\n", dw, dh);
                for (size_t i = 0, n = size_t(dw)*dh; i < n; ++i) {
                    fputc(buf[i*4+2], f);  // R (from BGRA)
                    fputc(buf[i*4+1], f);  // G
                    fputc(buf[i*4+0], f);  // B
                }
                fclose(f);
            }
            qInfo() << "[diag] DUMPED /tmp/spike_aov.ppm + /tmp/spike_drawable.ppm"
                    << "eye(" << m_eye[0] << m_eye[1] << m_eye[2] << ")";
        }

        if (m_diagFrames < 4) {
            // Present-side bisect diagnostic: read the PRESENTED drawable back
            // and apply the same differing-pixels metric as the AOV readback.
            id<MTLTexture> dt = drawable.texture;
            const int dw = int(dt.width), dh = int(dt.height);
            std::vector<unsigned char> buf(size_t(dw) * dh * 4);
            [dt getBytes:buf.data() bytesPerRow:(NSUInteger)dw*4
                fromRegion:MTLRegionMake2D(0, 0, dw, dh) mipmapLevel:0];
            size_t differing = 0;
            for (size_t i = 1, n = size_t(dw)*dh; i < n; ++i)
                if (memcmp(&buf[i*4], &buf[0], 4) != 0) ++differing;
            qInfo() << "[diag] present" << dw << "x" << dh
                    << "srcTex:" << (qulonglong)(uintptr_t)srcTex
                    << "layerAttached:" << (m_layer.superlayer != nil)
                    << "drawablePixelsDiffering:" << qulonglong(differing)
                    << "converged:" << m_engine->IsConverged();
            ++m_diagFrames;
        }
    }

private:
    UsdStageRefPtr                      m_stage;
    std::unique_ptr<UsdImagingGLEngine> m_engine;

    id<MTLDevice>              m_device   = nil;
    id<MTLCommandQueue>        m_queue    = nil;
    CAMetalLayer             *m_layer     = nil;
    id<MTLRenderPipelineState> m_pipeline = nil;

    int m_pw = 0, m_ph = 0, m_diagFrames = 0;
    long m_frameCount = 0;
    bool m_dumpRequested = false;

    GfVec3d  m_eye    { 4.0, 4.0, 3.0 };
    GfVec3d  m_center { 0.0, 0.0, 0.0 };
    GfVec3d  m_up     { 0.0, 0.0, 1.0 };   // Z-up
    GfFrustum  m_frustum;
    GfMatrix4d m_viewMatrix;

    QTimer  m_heartbeat;
    QString m_title;
    QString m_hudText;
    QPointF m_lastMousePos;
};

// ---------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("usd_qt_spike — Qt 6 + OpenUSD Hydra, macOS Metal-native present");
    parser.addHelpOption();
    QCommandLineOption twoOpt("two", "Open two windows sharing the same stage and Hgi.");
    parser.addOption(twoOpt);
    parser.process(app);

    UsdStageRefPtr stage = buildTestStage();

    SpikeView w1(stage, "SpikeWidget #1");
    w1.show();

    std::unique_ptr<SpikeView> w2;
    if (parser.isSet(twoOpt)) {
        w2 = std::make_unique<SpikeView>(stage, "SpikeWidget #2");
        w2->show();
    }

    return app.exec();
}

#include "main.moc"
