//
// usd-intro / UsdScene: authors an OpenUSD stage for the token ring and
// updates it live from simulation events.
//
// Demonstrates the OSG-replacement architecture end to end:
//   - the model authors USD (Z-up, metersPerUnit=1; Decision Q8),
//   - the stage is handed to Qtenv through cOsgCanvas::setScene() via the
//     renderer-neutral cScene3DNode handle,
//   - the oppqtenv-usd plugin renders it with Hydra Storm (Metal on macOS,
//     HgiGL elsewhere),
//   - prim->module bindings make 3D picking select simulation objects,
//   - refreshDisplay() drives smooth sim-time animation (orbiter), and
//     signal listeners drive event-based updates (token hops).
//

#include <omnetpp.h>

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usd/usdGeom/sphere.h>
#include <pxr/usd/usdGeom/cylinder.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/vt/array.h>

// completes cScene3DNode for the USD backend (model-side authoring header)
#include "usdscenehandle.h"

#include <cmath>

using namespace omnetpp;
PXR_NAMESPACE_USING_DIRECTIVE

class UsdScene : public cSimpleModule, public cListener
{
  private:
    static constexpr int numNodes = 4;
    static constexpr double ringRadius = 4.0;

    cScene3DNode *handle = nullptr;     // owned by the canvas (ref-counted)
    UsdStageRefPtr stage;

    UsdGeomSphere nodeSpheres[numNodes];
    UsdGeomSphere tokenSphere;
    UsdGeomXformOp tokenTranslateOp;
    UsdGeomXformOp orbiterTranslateOp;
    int activeNode = 0;

    static GfVec3d nodePos(int i)
    {
        const double a = 2.0 * M_PI * i / numNodes;
        return GfVec3d(ringRadius * std::cos(a), ringRadius * std::sin(a), 0.5);
    }

  protected:
    virtual void initialize() override
    {
        buildStage();

        // hand the scene to the (Qtenv) 3D viewer; headless envs no-op this
        cOsgCanvas *canvas = getParentModule()->getOsgCanvas();
        canvas->setClearColor(cOsgCanvas::Color(40, 44, 58));
        canvas->setGenericViewpoint(cOsgCanvas::Viewpoint(
            cOsgCanvas::Vec3d(9, 9, 6),     // eye
            cOsgCanvas::Vec3d(0, 0, 0),     // center
            cOsgCanvas::Vec3d(0, 0, 1)));   // up (Z-up)
        canvas->setScene(handle);

        // listen for token hops anywhere in the network
        getSimulation()->getSystemModule()->subscribe("tokenHop", this);
    }

    void buildStage()
    {
        stage = UsdStage::CreateInMemory();
        UsdGeomSetStageUpAxis(stage, UsdGeomTokens->z);     // Z-up (Decision Q8)
        UsdGeomSetStageMetersPerUnit(stage, 1.0);           // meters

        UsdGeomXform::Define(stage, SdfPath("/World"));
        handle = new cScene3DNode(stage);

        // ground plane: 16x16 m quad, dark gray
        UsdGeomMesh ground = UsdGeomMesh::Define(stage, SdfPath("/World/Ground"));
        VtArray<GfVec3f> pts = { GfVec3f(-8, -8, 0), GfVec3f(8, -8, 0),
                                 GfVec3f(8, 8, 0),   GfVec3f(-8, 8, 0) };
        ground.GetPointsAttr().Set(pts);
        ground.GetFaceVertexCountsAttr().Set(VtArray<int>{ 4 });
        ground.GetFaceVertexIndicesAttr().Set(VtArray<int>{ 0, 1, 2, 3 });
        ground.GetDoubleSidedAttr().Set(true);
        setColor(ground.GetPrim(), GfVec3f(0.28f, 0.30f, 0.34f));

        // ring nodes: one sphere per TokenNode, bound for picking
        static const GfVec3f idleColor(0.25f, 0.45f, 0.85f);
        for (int i = 0; i < numNodes; i++) {
            SdfPath path(std::string("/World/Node") + std::to_string(i));
            nodeSpheres[i] = UsdGeomSphere::Define(stage, path);
            nodeSpheres[i].GetRadiusAttr().Set(0.6);
            UsdGeomXformable(nodeSpheres[i].GetPrim()).AddTranslateOp().Set(nodePos(i));
            setColor(nodeSpheres[i].GetPrim(), idleColor);

            // 3D picking -> the actual TokenNode module
            cModule *mod = getParentModule()->getSubmodule("node", i);
            handle->bindObject(path, mod);
        }

        // the token: a small bright sphere sitting on the active node
        tokenSphere = UsdGeomSphere::Define(stage, SdfPath("/World/Token"));
        tokenSphere.GetRadiusAttr().Set(0.25);
        tokenTranslateOp = UsdGeomXformable(tokenSphere.GetPrim()).AddTranslateOp();
        tokenTranslateOp.Set(nodePos(0) + GfVec3d(0, 0, 1.0));
        setColor(tokenSphere.GetPrim(), GfVec3f(1.0f, 0.85f, 0.1f));

        // a satellite orbiting the ring, driven smoothly by simulation time
        UsdGeomSphere orbiter = UsdGeomSphere::Define(stage, SdfPath("/World/Orbiter"));
        orbiter.GetRadiusAttr().Set(0.3);
        orbiterTranslateOp = UsdGeomXformable(orbiter.GetPrim()).AddTranslateOp();
        orbiterTranslateOp.Set(GfVec3d(6, 0, 2));
        setColor(orbiter.GetPrim(), GfVec3f(0.9f, 0.35f, 0.25f));
        handle->bindObject(SdfPath("/World/Orbiter"), this);
    }

    static void setColor(const UsdPrim& prim, const GfVec3f& color)
    {
        UsdGeomPrimvarsAPI pv(prim);
        UsdGeomPrimvar c = pv.CreatePrimvar(TfToken("displayColor"),
            SdfValueTypeNames->Color3fArray, UsdGeomTokens->constant);
        c.Set(VtArray<GfVec3f>{ color });
    }

    // token hop: recolor nodes, move the token sphere (event-driven update)
    virtual void receiveSignal(cComponent *src, simsignal_t id, intval_t value, cObject *details) override
    {
        static const GfVec3f idleColor(0.25f, 0.45f, 0.85f);
        static const GfVec3f activeColor(0.3f, 0.95f, 0.4f);

        setColor(nodeSpheres[activeNode].GetPrim(), idleColor);
        activeNode = (int)value % numNodes;
        setColor(nodeSpheres[activeNode].GetPrim(), activeColor);
        tokenTranslateOp.Set(nodePos(activeNode) + GfVec3d(0, 0, 1.0));
    }

    // smooth sim-time animation (called by the GUI between events)
    virtual void refreshDisplay() const override
    {
        const double t = simTime().dbl();
        const double a = 0.5 * t;   // rad/s
        const_cast<UsdScene *>(this)->orbiterTranslateOp.Set(
            GfVec3d(6.0 * std::cos(a), 6.0 * std::sin(a), 2.0 + 0.5 * std::sin(2 * a)));
    }

    virtual void handleMessage(cMessage *msg) override
    {
        // no messages of our own
        delete msg;
    }
};

Define_Module(UsdScene);
