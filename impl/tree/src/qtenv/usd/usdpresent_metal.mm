//==========================================================================
//  USDPRESENT_METAL.MM - part of
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

// macOS Metal-native presenter for UsdViewer (Decision D6 / Risk R3).
//
// Validated end-to-end by the de-risking spike (impl/spike in the
// omnetpp-osg-replacement repo). Key facts encoded here:
//  - USD's hgiInterop is OpenGL-destination-only; on macOS (GL capped at 4.1,
//    HgiGL needs >= 4.5) the ONLY working path is HgiMetal + our own present.
//  - The CAMetalLayer must be a SUBLAYER of the host view's backing layer
//    (QWindow::MetalSurface and layer replacement both crash Qt's QNSView).
//  - Present the render buffer's own resolved texture (GetResource(false)),
//    NOT GetAovTexture's task-context texture (stale after picks).
//  - A per-frame synchronizing AOV readback (Map/Unmap) is load-bearing for
//    present freshness (16/16 instrumented frames bit-exact with it, stale
//    without). TODO(M4+): replace with an MTLSharedEvent fence.

#include <QtWidgets/QWidget>
#include <QtCore/QDebug>

#include <pxr/pxr.h>
#include <pxr/usdImaging/usdImagingGL/engine.h>
#include <pxr/imaging/hgi/hgi.h>
#include <pxr/imaging/hgi/texture.h>
#include <pxr/imaging/hgiMetal/hgi.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/renderBuffer.h>
#include <pxr/base/vt/value.h>

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <AppKit/AppKit.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace omnetpp {
namespace qtenv {
namespace usdmetal {

// Fullscreen-triangle blit: samples the Hydra color AOV (RGBA16F) into the
// layer drawable (BGRA8); V-flipped (the AOV is bottom-up).
static const char *kPresentMSL = R"MSL(
#include <metal_stdlib>
using namespace metal;
struct VOut { float4 pos [[position]]; float2 uv; };
vertex VOut vmain(uint vid [[vertex_id]]) {
    float2 p = float2((vid << 1) & 2, vid & 2);
    VOut o;
    o.pos = float4(p * 2.0 - 1.0, 0.0, 1.0);
    o.uv  = float2(p.x, 1.0 - p.y);
    return o;
}
fragment float4 fmain(VOut in [[stage_in]], texture2d<float> tex [[texture(0)]]) {
    constexpr sampler s(coord::normalized, filter::linear, address::clamp_to_edge);
    return tex.sample(s, in.uv);
}
)MSL";

struct MetalPresenter {
    CAMetalLayer              *layer    = nil;
    id<MTLDevice>              device   = nil;
    id<MTLCommandQueue>        queue    = nil;
    id<MTLRenderPipelineState> pipeline = nil;
    HgiMetal                  *hgiMetal = nullptr;  // the shared Hgi (engine->GetHgi()
                                                    // returns null for driver-provided Hgis in 25.11)
};

static void updateLayerGeometry(MetalPresenter *p, QWidget *host)
{
    if (!p->layer)
        return;
    const qreal dpr = host->devicePixelRatioF();
    const int pw = std::max(1, int(host->width() * dpr));
    const int ph = std::max(1, int(host->height() * dpr));
    p->layer.frame = CGRectMake(0, 0, host->width(), host->height());
    p->layer.contentsScale = dpr;
    p->layer.drawableSize = CGSizeMake(pw, ph);
}

void *createPresenter(QWidget *host, Hgi *hgi)
{
    // NOTE: static_cast after a name check, NOT dynamic_cast — RTTI across the
    // dlopen'd plugin / usd_ms boundary is unreliable (the spike validated the
    // static_cast pattern).
    qInfo() << "usdmetal: createPresenter, hgi:" << (void*)hgi
            << "api:" << (hgi ? hgi->GetAPIName().GetText() : "(null)");
    if (!hgi || hgi->GetAPIName() != HgiTokens->Metal) {
        qInfo() << "usdmetal: Hgi backend is not Metal - cannot present";
        return nullptr;
    }
    HgiMetal *hgiMetal = static_cast<HgiMetal *>(hgi);

    auto *p = new MetalPresenter;
    p->hgiMetal = hgiMetal;
    p->device = hgiMetal->GetPrimaryDevice();
    p->queue  = hgiMetal->GetQueue();
    qInfo() << "usdmetal: device" << (__bridge void*)p->device << "queue" << (__bridge void*)p->queue;

    // CAMetalLayer as a SUBLAYER of the host's backing layer; never replace
    // or reuse Qt's own layer (verified crash in QNSView displayLayer:).
    NSView *view = (__bridge NSView *)reinterpret_cast<void *>(host->winId());
    qInfo() << "usdmetal: host NSView" << (__bridge void*)view;
    view.wantsLayer = YES;
    p->layer = [CAMetalLayer layer];
    p->layer.device = p->device;
    p->layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    p->layer.framebufferOnly = YES;
    if (view.layer)
        [view.layer addSublayer:p->layer];
    else {
        view.layer = p->layer;
        view.wantsLayer = YES;
    }
    qInfo() << "usdmetal: layer attached:" << (p->layer.superlayer != nil);

    NSError *err = nil;
    id<MTLLibrary> lib = [p->device newLibraryWithSource:@(kPresentMSL) options:nil error:&err];
    if (!lib) {
        qInfo() << "usdmetal: MSL compile failed:"
                << (err ? err.localizedDescription.UTF8String : "(no error info)");
        delete p;
        return nullptr;
    }
    MTLRenderPipelineDescriptor *d = [[MTLRenderPipelineDescriptor alloc] init];
    d.vertexFunction   = [lib newFunctionWithName:@"vmain"];
    d.fragmentFunction = [lib newFunctionWithName:@"fmain"];
    d.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    p->pipeline = [p->device newRenderPipelineStateWithDescriptor:d error:&err];
    if (!p->pipeline) {
        qInfo() << "usdmetal: pipeline failed:"
                << (err ? err.localizedDescription.UTF8String : "(no error info)");
        delete p;
        return nullptr;
    }

    updateLayerGeometry(p, host);
    qInfo() << "usdmetal: presenter ready";
    return p;
}

void resizePresenter(void *pv, QWidget *host)
{
    if (auto *p = static_cast<MetalPresenter *>(pv))
        updateLayerGeometry(p, host);
}

void present(void *pv, UsdImagingGLEngine *engine, QWidget *host)
{
    auto *p = static_cast<MetalPresenter *>(pv);
    if (!p || !p->layer || !engine)
        return;

    HgiMetal *hgiMetal = p->hgiMetal;   // stored at creation; do NOT use engine->GetHgi()
    if (!hgiMetal)
        return;

    // Flush Hydra's pending Metal work before touching the AOV.
    hgiMetal->CommitPrimaryCommandBuffer(HgiMetal::CommitCommandBuffer_WaitUntilCompleted);

    static int diagBudget = 8;   // a few release-visible diagnostics, then quiet

    HdRenderBuffer *colorRb = engine->GetAovRenderBuffer(HdAovTokens->color);
    if (!colorRb) {
        if (diagBudget > 0) { --diagBudget; qWarning() << "usdmetal: no color render buffer"; }
        return;
    }
    colorRb->Resolve();

    // SYNCHRONIZING READBACK — load-bearing, do not remove (see file header).
    if (colorRb->Map())
        colorRb->Unmap();

    // Present the render buffer's own resolved texture.
    HgiTextureHandle colorH;
    VtValue res = colorRb->GetResource(/*multiSampled=*/false);
    if (res.IsHolding<HgiTextureHandle>())
        colorH = res.UncheckedGet<HgiTextureHandle>();
    if (!colorH)
        colorH = engine->GetAovTexture(HdAovTokens->color);  // fallback
    if (!colorH) {
        if (diagBudget > 0) { --diagBudget; qWarning() << "usdmetal: no color AOV texture"; }
        return;
    }
    id<MTLTexture> srcTex =
        (__bridge id<MTLTexture>)reinterpret_cast<void *>(colorH->GetRawResource());
    if (!srcTex) {
        if (diagBudget > 0) { --diagBudget; qWarning() << "usdmetal: null MTLTexture"; }
        return;
    }

    updateLayerGeometry(p, host);
    id<CAMetalDrawable> drawable = [p->layer nextDrawable];
    if (!drawable) {
        if (diagBudget > 0) { --diagBudget; qWarning() << "usdmetal: nextDrawable nil (layer "
            << (p->layer.superlayer ? "attached" : "DETACHED") << ")"; }
        return;
    }
    if (diagBudget > 0) { --diagBudget;
        qInfo() << "usdmetal: presenting" << int(p->layer.drawableSize.width) << "x"
                << int(p->layer.drawableSize.height)
                << "layer" << (p->layer.superlayer ? "attached" : "DETACHED"); }

    id<MTLCommandBuffer> cb = [p->queue commandBuffer];
    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture     = drawable.texture;
    pass.colorAttachments[0].loadAction  = MTLLoadActionClear;
    pass.colorAttachments[0].clearColor  = MTLClearColorMake(0, 0, 0, 1);
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    id<MTLRenderCommandEncoder> enc = [cb renderCommandEncoderWithDescriptor:pass];
    [enc setRenderPipelineState:p->pipeline];
    [enc setFragmentTexture:srcTex atIndex:0];
    [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [enc endEncoding];
    [cb presentDrawable:drawable];
    [cb commit];
    // Frame pacing — empirically required (spike: stale presents without it).
    [cb waitUntilCompleted];
}

void destroyPresenter(void *pv)
{
    auto *p = static_cast<MetalPresenter *>(pv);
    if (!p)
        return;
    if (p->layer)
        [p->layer removeFromSuperlayer];
    delete p;   // ARC releases the ObjC members
}

}  // namespace usdmetal
}  // namespace qtenv
}  // namespace omnetpp
