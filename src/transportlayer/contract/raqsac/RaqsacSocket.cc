
#include <inet/common/packet/Message.h>
#include <inet/common/ProtocolTag_m.h>
#include <inet/applications/common/SocketTag_m.h>
#include <inet/common/Protocol.h>
#include "RaqsacSocket.h"

namespace inet {

RaqsacSocket::RaqsacSocket()
{
    // don't allow user-specified connIds because they may conflict with
    // automatically assigned ones.
    connId = getEnvir()->getUniqueNumber();
}

RaqsacSocket::~RaqsacSocket()
{
    if (cb) {
        cb->socketDeleted(this);
        cb = nullptr;
    }
}

const char* RaqsacSocket::stateName(RaqsacSocket::State state)
{
#define CASE(x)    case x: \
        s = #x; break
    const char *s = "unknown";
    switch (state) {
        CASE(NOT_BOUND)
;            CASE(BOUND);
            CASE(LISTENING);
            CASE(CONNECTING);
            CASE(CONNECTED);
        }
    return s;
#undef CASE
}

void RaqsacSocket::sendToRaqsac(cMessage *msg, int connId)
{
    if (!gateToRaqsac)
        throw cRuntimeError("RaqsacSocket: setOutputGate() must be invoked before socket can be used");

    auto &tags = getTags(msg);
    tags.addTagIfAbsent<DispatchProtocolReq>()->setProtocol(&Protocol::raqsac);
    tags.addTagIfAbsent<SocketReq>()->setSocketId(connId == -1 ? this->connId : connId);
    check_and_cast<cSimpleModule*>(gateToRaqsac->getOwnerModule())->send(msg, gateToRaqsac);
}

void RaqsacSocket::bind(L3Address lAddr, int lPort)
{
    if (sockstate != NOT_BOUND)
        throw cRuntimeError("RaqsacSocket::bind(): socket already bound");

    // allow -1 here, to make it possible to specify address only
    if ((lPort < 0 || lPort > 65535) && lPort != -1)
        throw cRuntimeError("RaqsacSocket::bind(): invalid port number %d", lPort);
    localAddr = lAddr;
    localPrt = lPort;
    sockstate = BOUND;
}

void RaqsacSocket::listen(bool fork)
{
    if (sockstate != BOUND)
        throw cRuntimeError(sockstate == NOT_BOUND ? "RaqsacSocket: must call bind() before listen()" : "RaqsacSocket::listen(): connect() or listen() already called");

    auto request = new Request("PassiveOPEN", RAQSAC_C_OPEN_PASSIVE);
    RaqsacOpenCommand *openCmd = new RaqsacOpenCommand();
    openCmd->setLocalAddr(localAddr);
    openCmd->setLocalPort(localPrt);
    openCmd->setRaqsacAlgorithmClass(raqsacAlgorithmClass.c_str());
    request->setControlInfo(openCmd);
    sendToRaqsac(request);
    sockstate = LISTENING;
}

void RaqsacSocket::accept(int socketId)
{
    throw cRuntimeError("RaqsacSocket::accept(): never called");
}

void RaqsacSocket::connect(L3Address localAddress, L3Address remoteAddress, int remotePort, unsigned int numSymbolsToSend, unsigned int priorityValue)
{
    if (sockstate != NOT_BOUND && sockstate != BOUND)
        throw cRuntimeError("RaqsacSocket::connect(): connect() or listen() already called (need renewSocket()?)");

    if (remotePort < 0 || remotePort > 65535)
        throw cRuntimeError("RaqsacSocket::connect(): invalid remote port number %d", remotePort);

    auto request = new Request("ActiveOPEN", RAQSAC_C_OPEN_ACTIVE);
    localAddr = localAddress;
    remoteAddr = remoteAddress;
    remotePrt = remotePort;

    RaqsacOpenCommand *openCmd = new RaqsacOpenCommand();
    openCmd->setLocalAddr(localAddr);
    openCmd->setLocalPort(localPrt);
    openCmd->setRemoteAddr(remoteAddr);
    openCmd->setRemotePort(remotePrt);
    openCmd->setRaqsacAlgorithmClass(raqsacAlgorithmClass.c_str());
    openCmd->setNumSymbolsToSend(numSymbolsToSend);
    openCmd->setPriorityValue(priorityValue);
    request->setControlInfo(openCmd);
    sendToRaqsac(request);
    sockstate = CONNECTING;
    EV_INFO << "Socket Connection Finished" << endl;
}

void RaqsacSocket::send(Packet *msg)
{
    throw cRuntimeError("RaqsacSocket::send(): never called by application - hack where RaQSac handles all data");
}

void RaqsacSocket::close()
{
    throw cRuntimeError("RaqsacSocket::close(): never called by application");
}

void RaqsacSocket::abort()
{
    throw cRuntimeError("RaqsacSocket::abort(): never called by application - hack where RaQSac handles all data");
}

void RaqsacSocket::destroy()
{
    throw cRuntimeError("RaqsacSocket::destroy(): never called by application - hack where RaQSac handles all data");
}

void RaqsacSocket::renewSocket()
{
    throw cRuntimeError("RaqsacSocket::renewSocket(): not needed as the socket should never be closed to begin with");
}

bool RaqsacSocket::isOpen() const
{
    throw cRuntimeError("RaqsacSocket::isOpen(): never called");
}

bool RaqsacSocket::belongsToSocket(cMessage *msg) const
{
    auto &tags = getTags(msg);
    auto socketInd = tags.findTag<SocketInd>();
    return socketInd != nullptr && socketInd->getSocketId() == connId;
}

void RaqsacSocket::setCallback(ICallback *callback)
{
    cb = callback;
}

void RaqsacSocket::processMessage(cMessage *msg)
{
    ASSERT(belongsToSocket(msg));
    RaqsacConnectInfo *connectInfo;

    switch (msg->getKind()) {
        case RAQSAC_I_DATA:
            if (cb)
                cb->socketDataArrived(this, check_and_cast<Packet*>(msg), false); // see NdpBasicClientApp::socketDataArrived
            else
                delete msg;

            break;

        case RAQSAC_I_ESTABLISHED:
            // Note: this code is only for sockets doing active open, and nonforking
            // listening sockets. For a forking listening sockets, NDP_I_ESTABLISHED
            // carries a new connId which won't match the connId of this RaqsacSocket,
            // so you won't get here. Rather, when you see NDP_I_ESTABLISHED, you'll
            // want to create a new RaqsacSocket object via new RaqsacSocket(msg).
            sockstate = CONNECTED;
            connectInfo = check_and_cast<RaqsacConnectInfo*>(msg->getControlInfo());
            localAddr = connectInfo->getLocalAddr();
            remoteAddr = connectInfo->getRemoteAddr();
            localPrt = connectInfo->getLocalPort();
            remotePrt = connectInfo->getRemotePort();
            if (cb)
                cb->socketEstablished(this);
            delete msg;
            break;
        default:
            throw cRuntimeError("RaqsacSocket: invalid msg kind %d, one of the NDP_I_xxx constants expected", msg->getKind());
    }
}

} // namespace inet

