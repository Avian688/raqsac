#include <iostream>
#include <random>
#include <inet/common/lifecycle/ModuleOperations.h>
#include <inet/common/ModuleAccess.h>
#include <inet/common/TimeTag_m.h>

#include "../raqsacapp/GenericAppMsgRaqsac_m.h"
#include "RaqsacBasicClientApp.h"
namespace inet {

#define MSGKIND_CONNECT    0

Define_Module(RaqsacBasicClientApp);

RaqsacBasicClientApp::~RaqsacBasicClientApp()
{
    cancelAndDelete(timeoutMsg);
}

void RaqsacBasicClientApp::initialize(int stage)
{
    EV_TRACE << "RaqsacBasicClientApp::initialize stage " << stage << endl;
    RaqsacAppBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        startTime = par("startTime");
        stopTime = par("stopTime");
        if (stopTime >= SIMTIME_ZERO && stopTime < startTime)
            throw cRuntimeError("Invalid startTime/stopTime parameters");
        //timeoutMsg = new cMessage("timer");
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER) {
            timeoutMsg = new cMessage("timer");
            nodeStatus = dynamic_cast<NodeStatus *>(findContainingNode(this)->getSubmodule("status"));
            if (isNodeUp()) {
                timeoutMsg->setKind(MSGKIND_CONNECT);
                scheduleAt(startTime, timeoutMsg);
            }
    }
    // TODO update timer to make it more up to date
}

bool RaqsacBasicClientApp::isNodeUp() {
    return !nodeStatus || nodeStatus->getState() == NodeStatus::UP;
}

void RaqsacBasicClientApp::handleStartOperation(LifecycleOperation *operation)
{
    simtime_t now = simTime();
    simtime_t start = std::max(startTime, now);
    if (timeoutMsg && ((stopTime < SIMTIME_ZERO) || (start < stopTime) || (start == stopTime && startTime == stopTime))) {
        timeoutMsg->setKind(MSGKIND_CONNECT);
        scheduleAt(start, timeoutMsg);
    }
}

void RaqsacBasicClientApp::handleStopOperation(LifecycleOperation *operation)
{
    cancelEvent(timeoutMsg);
    if (socket.getState() == RaqsacSocket::CONNECTED || socket.getState() == RaqsacSocket::CONNECTING){
        close();
    }
}

void RaqsacBasicClientApp::handleCrashOperation(LifecycleOperation *operation)
{
    throw cRuntimeError("RaqsacBasicClientApp::handleCrashOperation - not implemented");
}

void RaqsacBasicClientApp::handleTimer(cMessage *msg)
{
    // Added MOH send requests based on a timer
    switch (msg->getKind()) {
        case MSGKIND_CONNECT:
            connect();    // active OPEN
            break;
        default:
            throw cRuntimeError("Invalid timer msg: kind=%d", msg->getKind());
    }
}

void RaqsacBasicClientApp::socketEstablished(RaqsacSocket *socket)
{
    RaqsacAppBase::socketEstablished(socket);
}

void RaqsacBasicClientApp::rescheduleOrDeleteTimer(simtime_t d, short int msgKind)
{
    cancelEvent(timeoutMsg);

    if (stopTime < SIMTIME_ZERO || d < stopTime) {
        timeoutMsg->setKind(msgKind);
        scheduleAt(d, timeoutMsg);
    }
    else {
        delete timeoutMsg;
        timeoutMsg = nullptr;
    }
}

void RaqsacBasicClientApp::close()
{
    RaqsacAppBase::close();
    cancelEvent(timeoutMsg);
}
void RaqsacBasicClientApp::socketClosed(RaqsacSocket *socket)
{
    RaqsacAppBase::socketClosed(socket);
}

void RaqsacBasicClientApp::socketFailure(RaqsacSocket *socket, int code)
{
    RaqsacAppBase::socketFailure(socket, code);
    // reconnect after a delay
    if (timeoutMsg) {
        simtime_t d = simTime() + (simtime_t) par("reconnectInterval");
        rescheduleOrDeleteTimer(d, MSGKIND_CONNECT);
    }
}

}    // namespace inet
