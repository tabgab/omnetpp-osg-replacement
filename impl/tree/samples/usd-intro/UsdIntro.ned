//
// usd-intro: first demo of the OpenUSD 3D viewer (OSG replacement).
//
// A 4-node token ring; the UsdScene module authors an OpenUSD stage
// (Z-up, meters) and updates it live as the token hops, demonstrating
// model->USD authoring, Hydra rendering in Qtenv, and picking.
//

simple TokenNode
{
    parameters:
        double hopDelay @unit(s) = default(0.4s);
        @display("i=block/routing");
    gates:
        input in;
        output out;
}

simple UsdScene
{
    parameters:
        @display("i=block/cogwheel");
}

network UsdIntro
{
    @display("bgb=400,300");
    submodules:
        node[4]: TokenNode;
        scene: UsdScene;
    connections:
        for i=0..3 {
            node[i].out --> node[(i+1)%4].in;
        }
}
