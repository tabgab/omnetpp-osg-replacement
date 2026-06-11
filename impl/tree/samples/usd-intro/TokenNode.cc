//
// usd-intro / TokenNode: a trivial token-ring node.
// Node 0 injects the token; each node holds it for hopDelay, then forwards.
// Each hop emits the "tokenHop" signal (value = receiving node's index),
// which the UsdScene module visualizes in the 3D scene.
//

#include <omnetpp.h>

using namespace omnetpp;

class TokenNode : public cSimpleModule
{
  protected:
    simsignal_t tokenHopSignal;

    virtual void initialize() override
    {
        tokenHopSignal = registerSignal("tokenHop");
        if (getIndex() == 0) {
            cMessage *token = new cMessage("token");
            scheduleAt(simTime() + par("hopDelay"), token);
            emit(tokenHopSignal, (long)getIndex());
        }
    }

    virtual void handleMessage(cMessage *msg) override
    {
        if (msg->isSelfMessage()) {
            // holding time over: pass the token to the next node
            send(msg, "out");
        }
        else {
            // token arrived: announce, hold, then forward
            EV << "Token arrived at node " << getIndex() << "\n";
            emit(tokenHopSignal, (long)getIndex());
            scheduleAt(simTime() + par("hopDelay"), msg);
        }
    }
};

Define_Module(TokenNode);
