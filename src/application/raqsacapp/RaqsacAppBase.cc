#include "RaqsacAppBase.h"

#include <inet/networklayer/common/L3AddressResolver.h>
#include  "../../transportlayer/contract/raqsac/RaqsacSocket.h"

namespace inet {

void RaqsacAppBase::initialize(int stage)
{
    EV_TRACE << "RaqsacAppBase::initialize stage " << stage;
    ApplicationBase::initialize(stage);
    if (stage == INITSTAGE_APPLICATION_LAYER) {
        // parameters
        const char *localAddress = par("localAddress");
        int localPort = par("localPort");

        socket.bind(*localAddress ? L3AddressResolver().resolve(localAddress) : L3Address(), localPort);
        socket.setCallback(this);
        socket.setOutputGate(gate("socketOut"));
    }
}

void RaqsacAppBase::handleMessageWhenUp(cMessage *msg)
{
    EV_TRACE << "RaqsacAppBase::handleMessageWhenUp" << endl;
    if (msg->isSelfMessage()) {
        handleTimer(msg);
    }
    else {
        socket.processMessage(msg);
    }
}

void RaqsacAppBase::connect()
{
    EV_TRACE << "RaqsacAppBase::connect" << endl;

    int numSymbolsToSend = par("numSymbolsToSend").intValue();
    int priorityValue = 0;
    if(par("priorityValue").intValue() == 0){
        priorityValue = getPriorityValue(numSymbolsToSend);
    }
    else{
        priorityValue = par("priorityValue").intValue();
    }

    // connect
    const char *connectAddress = par("connectAddress");
    int connectPort = par("connectPort");

    L3Address destination;
    L3AddressResolver().tryResolve(connectAddress, destination);

    // added by MOH
    const char *srcAddress = par("localAddress");
    L3Address localAddress;
    L3AddressResolver().tryResolve(srcAddress, localAddress);

    if (destination.isUnspecified()) {
        EV_ERROR << "Connecting to " << connectAddress << " port=" << connectPort << ": cannot resolve destination address\n";
    }
    else {
        EV_INFO << "Connecting to " << connectAddress << "(" << destination << ") port=" << connectPort << endl;

        socket.connect(localAddress, destination, connectPort, numSymbolsToSend, priorityValue);
        EV_INFO << "Connecting to mmmmm" << connectAddress << "(" << destination << ") port=" << connectPort << endl;
    }
}

void RaqsacAppBase::close()
{
    EV_INFO << "issuing CLOSE command\n";
    socket.close();
}

void RaqsacAppBase::socketEstablished(RaqsacSocket*)
{
    // *redefine* to perform or schedule first sending
    EV_INFO << "connected" << endl;
}

void RaqsacAppBase::socketDataArrived(RaqsacSocket*, Packet *msg, bool)
{
    // *redefine* to perform or schedule next sending
    delete msg;
}

void RaqsacAppBase::socketPeerClosed(RaqsacSocket *socket_)
{
    throw cRuntimeError("RaqsacAppBase::socketPeerClosed(): never called");
}

void RaqsacAppBase::socketClosed(RaqsacSocket*)
{
    // *redefine* to start another session etc.
    EV_INFO << "connection closed" << endl;
}

void RaqsacAppBase::socketFailure(RaqsacSocket*, int code)
{
    // subclasses may override this function, and add code try to reconnect after a delay.
    EV_WARN << "connection broken\n";
}

void RaqsacAppBase::finish()
{
    std::string modulePath = getFullPath();
}

// 0     --> 10KB    P=1
// 10KB  --> 100KB   P=2
// 100KB --> 1MB     P=3
// 1MB   --> 10MB    P=4
// otherwise (longflows)         P=0 (RaptorQBasicClientApp.ned --> int priorityValue = default(0);)
int RaqsacAppBase::getPriorityValue(int flowSize)
{
    int priorityValue;
    if (flowSize >= 1 && flowSize <= 7) {
        priorityValue = 1;
        return priorityValue;
    }

    if (flowSize >= 8 && flowSize <= 67) {
        priorityValue = 2;
        return priorityValue;
    }

    if (flowSize >= 68 && flowSize <= 667) {
        priorityValue = 3;
        return priorityValue;
    }

    if (flowSize >= 668 && flowSize <= 6667) {
        priorityValue = 4;
        return priorityValue;
    }

    return 0;
}

} // namespace inet
