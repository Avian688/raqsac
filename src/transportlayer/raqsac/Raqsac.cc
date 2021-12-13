//
// Copyright (C) 2004 Andras Varga
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#include <inet/applications/common/SocketTag_m.h>
#include <inet/common/IProtocolRegistrationListener.h>
#include <inet/common/ModuleAccess.h>
#include <inet/common/ProtocolTag_m.h>
#include <inet/common/checksum/TcpIpChecksum.h>
#include <inet/common/lifecycle/LifecycleOperation.h>
#include <inet/common/lifecycle/ModuleOperations.h>
#include <inet/common/lifecycle/NodeStatus.h>
#include <inet/common/packet/Message.h>
#include <inet/networklayer/common/EcnTag_m.h>
#include <inet/networklayer/common/IpProtocolId_m.h>
#include <inet/networklayer/common/L3AddressTag_m.h>
#include <inet/common/Protocol.h>
#include <inet/transportlayer/common/TransportPseudoHeader_m.h>

#ifdef WITH_IPv4
#include <inet/networklayer/ipv4/IcmpHeader_m.h>
#endif // ifdef WITH_IPv4

#ifdef WITH_IPv6
#include <inet/networklayer/icmpv6/Icmpv6Header_m.h>
#endif // ifdef WITH_IPv6

#define PACING_TIME 8  //    MTU/linkRate

#include "Raqsac.h"
#include "RaqsacConnection.h"
#include "raqsac_common/RaqsacHeader.h"
#include "../common/L4ToolsRaqsac.h"
#include "../contract/raqsac/RaqsacCommand_m.h"
namespace inet {
namespace raqsac {

Define_Module(Raqsac);

simsignal_t Raqsac::numRequestsRTOs = registerSignal("numRequestsRTOs");

Raqsac::~Raqsac()
{
    while (!raqsacAppConnMap.empty()) {
        auto i = raqsacAppConnMap.begin();
        i->second->deleteModule();
        raqsacAppConnMap.erase(i);
    }
}

void Raqsac::initialize(int stage)
{
    OperationalBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {

        lastEphemeralPort = EPHEMERAL_PORTRANGE_START;

        WATCH(lastEphemeralPort);
        WATCH_PTRMAP(raqsacConnMap);
        WATCH_PTRMAP(raqsacAppConnMap);
    }
    else if (stage == INITSTAGE_TRANSPORT_LAYER) {
        requestTimerMsg = new cMessage("requestTimerMsg");
        requestTimerMsg->setContextPointer(this);

        registerService(Protocol::raqsac, gate("appIn"), gate("ipIn"));
        registerProtocol(Protocol::raqsac, gate("ipOut"), gate("appOut"));
    }
}

void Raqsac::finish()
{
    if (requestTimerMsg->isScheduled()) {
        cancelEvent(requestTimerMsg);
    }
    delete requestTimerMsg;
    EV_INFO << getFullPath() << ": finishing with " << raqsacConnMap.size() << " connections open.\n";
}

void Raqsac::handleSelfMessage(cMessage *msg)
{
    if (msg == requestTimerMsg) {
        process_REQUEST_TIMER();
        if(!timerQueue.empty()){
            if(!requestTimerMsg->isScheduled()){
                scheduleAt(simTime() + timerQueue.front(), requestTimerMsg);
                timerQueue.pop();
            }
        }
    }
    else {
        RaqsacConnection *conn = (RaqsacConnection*) msg->getContextPointer();
        conn->processTimer(msg);
    }
}

void Raqsac::handleUpperCommand(cMessage *msg)
{
    int socketId = getTags(msg).getTag<SocketReq>()->getSocketId();
    RaqsacConnection *conn = findConnForApp(socketId);

    if (!conn) {
        conn = createConnection(socketId);

        // add into appConnMap here; it'll be added to connMap during processing
        // the OPEN command in TcpConnection's processAppCommand().
        raqsacAppConnMap[socketId] = conn;

        EV_INFO << "RaQSac connection created for " << msg << "\n";
    }

    if (!conn->processAppCommand(msg))
        removeConnection(conn);
}

void Raqsac::sendFromConn(cMessage *msg, const char *gatename, int gateindex)
{
    Enter_Method_Silent
    ();
    take(msg);
    send(msg, gatename, gateindex);
}

void Raqsac::handleUpperPacket(Packet *packet)
{
    handleUpperCommand(packet);
}
RaqsacConnection* Raqsac::findConnForApp(int socketId)
{
    auto i = raqsacAppConnMap.find(socketId);
    return i == raqsacAppConnMap.end() ? nullptr : i->second;
}

void Raqsac::handleLowerPacket(Packet *packet)
{
    EV_TRACE << "Raqsac::handleLowerPacket";
    EV_INFO << "Lower Packet Handled: " << packet->str() << std::endl;
    // must be a RaqsacHeader
    auto protocol = packet->getTag<PacketProtocolTag>()->getProtocol();
    if (protocol == &Protocol::raqsac) {
        auto raqsacHeader = packet->peekAtFront<RaqsacHeader>();
        L3Address srcAddr, destAddr;
        srcAddr = packet->getTag<L3AddressInd>()->getSrcAddress();
        destAddr = packet->getTag<L3AddressInd>()->getDestAddress();
        int ecn = 0;
        if (auto ecnTag = packet->findTag<EcnInd>())
            ecn = ecnTag->getExplicitCongestionNotification();
        ASSERT(ecn != -1);

        // process segment
        RaqsacConnection *conn = nullptr;
        conn = findConnForSegment(raqsacHeader, srcAddr, destAddr);
        if (conn) {
            bool ret = conn->processRaqsacSegment(packet, raqsacHeader, srcAddr, destAddr);
            if (!ret)
                removeConnection(conn);
        }
        else {
            segmentArrivalWhileClosed(packet, raqsacHeader, srcAddr, destAddr);
        }
    }
    else if (protocol == &Protocol::icmpv4 || protocol == &Protocol::icmpv6) {
        EV_DETAIL << "ICMP error received -- discarding\n"; // FIXME can ICMP packets really make it up to Tcp???
        delete packet;
    }
    else
        throw cRuntimeError("Unknown protocol: '%s'", (protocol != nullptr ? protocol->getName() : "<nullptr>"));
}

RaqsacConnection* Raqsac::createConnection(int socketId)
{
    auto moduleType = cModuleType::get("raqsac.transportlayer.raqsac.RaqsacConnection");
    char submoduleName[24];
    sprintf(submoduleName, "conn-%d", socketId);
    auto module = check_and_cast<RaqsacConnection*>(moduleType->createScheduleInit(submoduleName, this));
    module->initConnection(this, socketId);
    return module;
}

void Raqsac::removeConnection(RaqsacConnection *conn)
{
    EV_INFO << "Deleting RaQSac connection\n";

    raqsacAppConnMap.erase(conn->socketId);

    SockPair key2;
    key2.localAddr = conn->localAddr;
    key2.remoteAddr = conn->remoteAddr;
    key2.localPort = conn->localPort;
    key2.remotePort = conn->remotePort;
    raqsacConnMap.erase(key2);

    // IMPORTANT: usedEphemeralPorts.erase(conn->localPort) is NOT GOOD because it
    // deletes ALL occurrences of the port from the multiset.
    auto it = usedEphemeralPorts.find(conn->localPort);

    if (it != usedEphemeralPorts.end())
        usedEphemeralPorts.erase(it);

    //emit(RaqsacConnectionRemovedSignal, conn);
    conn->deleteModule();
}

RaqsacConnection* Raqsac::findConnForSegment(const Ptr<const RaqsacHeader> &raqsacseg, L3Address srcAddr, L3Address destAddr)
{
    SockPair key;
    key.localAddr = destAddr;
    key.remoteAddr = srcAddr;
    key.localPort = raqsacseg->getDestPort();
    key.remotePort = raqsacseg->getSrcPort();
    SockPair save = key;
    EV_TRACE << "Raqsac::findConnForSegment" << endl;

    // try with fully qualified SockPair
    auto i = raqsacConnMap.find(key);
    if (i != raqsacConnMap.end())
        return i->second;

    // try with localAddr missing (only localPort specified in passive/active open)
    key.localAddr = L3Address();
    i = raqsacConnMap.find(key);

    if (i != raqsacConnMap.end())
        return i->second;
    // try fully qualified local socket + blank remote socket (for incoming SYN)
    key = save;
    key.remoteAddr = L3Address();
    key.remotePort = -1;
    i = raqsacConnMap.find(key);
    if (i != raqsacConnMap.end())
        return i->second;

    // try with blank remote socket, and localAddr missing (for incoming SYN)
    key.localAddr = L3Address();
    i = raqsacConnMap.find(key);
    if (i != raqsacConnMap.end())
        return i->second;
    // given up
    return nullptr;
}

void Raqsac::segmentArrivalWhileClosed(Packet *packet, const Ptr<const RaqsacHeader> &raqsacseg, L3Address srcAddr, L3Address destAddr)
{
    auto moduleType = cModuleType::get("raqsac.transportlayer.raqsac.RaqsacConnection");
    const char *submoduleName = "conn-temp";
    auto module = check_and_cast<RaqsacConnection*>(moduleType->createScheduleInit(submoduleName, this));
    module->initConnection(this, -1);
    module->segmentArrivalWhileClosed(packet, raqsacseg, srcAddr, destAddr);
    module->deleteModule();
    delete packet;
}

ushort Raqsac::getEphemeralPort()
{
    // start at the last allocated port number + 1, and search for an unused one
    ushort searchUntil = lastEphemeralPort++;
    if (lastEphemeralPort == EPHEMERAL_PORTRANGE_END) { // wrap
        lastEphemeralPort = EPHEMERAL_PORTRANGE_START;
    }
    while (usedEphemeralPorts.find(lastEphemeralPort) != usedEphemeralPorts.end()) {
        if (lastEphemeralPort == searchUntil) // got back to starting point?
            throw cRuntimeError("Ephemeral port range %d..%d exhausted, all ports occupied", EPHEMERAL_PORTRANGE_START, EPHEMERAL_PORTRANGE_END);
        lastEphemeralPort++;
        if (lastEphemeralPort == EPHEMERAL_PORTRANGE_END) // wrap
            lastEphemeralPort = EPHEMERAL_PORTRANGE_START;
    }

    // found a free one, return it
    return lastEphemeralPort;
}

void Raqsac::addSockPair(RaqsacConnection *conn, L3Address localAddr, L3Address remoteAddr, int localPort, int remotePort)
{
    // update addresses/ports in TcpConnection
    SockPair key;
    key.localAddr = conn->localAddr = localAddr;
    key.remoteAddr = conn->remoteAddr = remoteAddr;
    key.localPort = conn->localPort = localPort;
    key.remotePort = conn->remotePort = remotePort;
    // make sure connection is unique
    auto it = raqsacConnMap.find(key);
    if (it != raqsacConnMap.end()) {
        // throw "address already in use" error
        if (remoteAddr.isUnspecified() && remotePort == -1)
            throw cRuntimeError("Address already in use: there is already a connection listening on %s:%d", localAddr.str().c_str(), localPort);
        else
            throw cRuntimeError("Address already in use: there is already a connection %s:%d to %s:%d", localAddr.str().c_str(), localPort, remoteAddr.str().c_str(), remotePort);
    }

    // then insert it into ncpConnMap
    raqsacConnMap[key] = conn;

    // mark port as used
    if (localPort >= EPHEMERAL_PORTRANGE_START && localPort < EPHEMERAL_PORTRANGE_END)
        usedEphemeralPorts.insert(localPort);
}

void Raqsac::updateSockPair(RaqsacConnection *conn, L3Address localAddr, L3Address remoteAddr, int localPort, int remotePort)
{
    // find with existing address/port pair...
    SockPair key;
    key.localAddr = conn->localAddr;
    key.remoteAddr = conn->remoteAddr;
    key.localPort = conn->localPort;
    key.remotePort = conn->remotePort;
    auto it = raqsacConnMap.find(key);

    ASSERT(it != raqsacConnMap.end() && it->second == conn);

    // ...and remove from the old place in RaqsacConnMap
    raqsacConnMap.erase(it);

    // then update addresses/ports, and re-insert it with new key into RaqsacConnMap
    key.localAddr = conn->localAddr = localAddr;
    key.remoteAddr = conn->remoteAddr = remoteAddr;
    ASSERT(conn->localPort == localPort);
    key.remotePort = conn->remotePort = remotePort;
    raqsacConnMap[key] = conn;

    EV_TRACE << "Raqsac::updateSockPair" << endl;
    // localPort doesn't change (see ASSERT above), so there's no need to update usedEphemeralPorts[].
}

void Raqsac::handleStartOperation(LifecycleOperation *operation)
{
    //FIXME implementation
}

void Raqsac::handleStopOperation(LifecycleOperation *operation)
{
    //FIXME close connections??? yes, because the applications may not close them!!!
    reset();
    delayActiveOperationFinish(par("stopOperationTimeout"));
    startActiveOperationExtraTimeOrFinish(par("stopOperationExtraTime"));
}

void Raqsac::handleCrashOperation(LifecycleOperation *operation)
{
    reset();
}

void Raqsac::reset()
{
    for (auto &elem : raqsacAppConnMap)
        elem.second->deleteModule();
    raqsacAppConnMap.clear();
    raqsacConnMap.clear();
    usedEphemeralPorts.clear();
    lastEphemeralPort = EPHEMERAL_PORTRANGE_START;
}

void Raqsac::refreshDisplay() const
{
    OperationalBase::refreshDisplay();
    if (getEnvir()->isExpressMode()) {
        // in express mode, we don't bother to update the display
        // (std::map's iteration is not very fast if map is large)
        getDisplayString().setTagArg("t", 0, "");
        return;
    }
    int numINIT = 0, numCLOSED = 0, numLISTEN = 0, numSYN_SENT = 0, numSYN_RCVD = 0, numESTABLISHED = 0, numCLOSE_WAIT = 0, numLAST_ACK = 0, numCLOSING = 0;

    for (auto &elem : raqsacAppConnMap) {
        int state = (elem).second->getFsmState();

        switch (state) {
            case RAQSAC_S_INIT:
                numINIT++;
                break;

            case RAQSAC_S_CLOSED:
                numCLOSED++;
                break;

            case RAQSAC_S_LISTEN:
                numLISTEN++;
                break;

            case RAQSAC_S_SYN_SENT:
                numSYN_SENT++;
                break;

            case RAQSAC_S_SYN_RCVD:
                numSYN_RCVD++;
                break;

            case RAQSAC_S_ESTABLISHED:
                numESTABLISHED++;
                break;
        }
    }

    char buf2[200];
    buf2[0] = '\0';

    if (numINIT > 0)
        sprintf(buf2 + strlen(buf2), "init:%d ", numINIT);
    if (numCLOSED > 0)
        sprintf(buf2 + strlen(buf2), "closed:%d ", numCLOSED);
    if (numLISTEN > 0)
        sprintf(buf2 + strlen(buf2), "listen:%d ", numLISTEN);
    if (numSYN_SENT > 0)
        sprintf(buf2 + strlen(buf2), "syn_sent:%d ", numSYN_SENT);
    if (numSYN_RCVD > 0)
        sprintf(buf2 + strlen(buf2), "syn_rcvd:%d ", numSYN_RCVD);
    if (numESTABLISHED > 0)
        sprintf(buf2 + strlen(buf2), "estab:%d ", numESTABLISHED);
    if (numCLOSE_WAIT > 0)
        sprintf(buf2 + strlen(buf2), "close_wait:%d ", numCLOSE_WAIT);
    if (numLAST_ACK > 0)
        sprintf(buf2 + strlen(buf2), "last_ack:%d ", numLAST_ACK);
    if (numCLOSING > 0)
        sprintf(buf2 + strlen(buf2), "closing:%d ", numCLOSING);

    getDisplayString().setTagArg("t", 0, buf2);
}

void Raqsac::printConnRequestMap()
{
    auto iterrr = requestCONNMap.begin();
    int index = 0;
    while (iterrr != requestCONNMap.end()) {
        index++;
        iterrr++;
    }

}

void Raqsac::sendFirstRequest()
{
    bool allEmpty = allPullQueuesEmpty();
    if (allEmpty == false) {
        requestTimer();
    }
}

bool Raqsac::allPullQueuesEmpty()
{
    int pullsQueueLength = 0;
    auto iter = requestCONNMap.begin();
    while (iter != requestCONNMap.end()) {
        pullsQueueLength = iter->second->getPullsQueueLength();
        if (pullsQueueLength > 0)
            return false;
        ++iter;
    }
    return true;
}

bool Raqsac::allConnFinished()
{
//     std::cout << "  allConnFinished ?   "     << "\n";
    bool connDone;

    auto iter = requestCONNMap.begin();
    int ii = 0;
    while (iter != requestCONNMap.end()) {
        connDone = iter->second->isConnFinished();
        if (connDone == false) {
            return false;
        }
        ++iter;
        ++ii;
    }
    cancelRequestTimer();
    return true;
}

void Raqsac::updateConnMap()
{
    std::cout << "  updateConnMap updateConnMap   " << "\n";
    a: bool connDone;
    auto iter = requestCONNMap.begin();

    while (iter != requestCONNMap.end()) {
        connDone = iter->second->isConnFinished();
        if (connDone == true) {
            requestCONNMap.erase(iter);
            goto a;
        }
        ++iter;
    }
}

void Raqsac::requestTimer()
{
    Enter_Method_Silent
    ("requestTimer");
    EV_INFO << "Timer being requested!" << endl;
    //cancelRequestTimer();
    if((requestTimerMsg->getOwner() != this) || requestTimerMsg->isScheduled()){
        simtime_t requestTime = SimTime(PACING_TIME, SIMTIME_US);
        timerQueue.push(requestTime);
    }
    else{
       simtime_t requestTime = (simTime() + SimTime( PACING_TIME, SIMTIME_US)); // pacing
       scheduleAt(requestTime, requestTimerMsg); // 0.000009
    }

}

void Raqsac::cancelRequestTimer()
{
    if (requestTimerMsg->isScheduled())
        cancelEvent(requestTimerMsg);
}

bool Raqsac::getNapState()
{
    return nap;
}

void Raqsac::process_REQUEST_TIMER()
{
    bool sendNewRequest = false;
    bool allEmpty = allPullQueuesEmpty();
    bool allDone = allConnFinished();

    if (allDone == true) {
        cancelRequestTimer();
    }
    else if (allDone == false && allEmpty == true) {
        ++times;
        nap = true;
    }
    else if (allDone == false && allEmpty == false) {
        times = 0;
        nap = false;
        while (sendNewRequest != true) {
            if (counter == requestCONNMap.size())
                counter = 0;
            auto iter = requestCONNMap.begin();
            std::advance(iter, counter);
            int pullsQueueLength = iter->second->getPullsQueueLength();
            if (pullsQueueLength > 0) {
                iter->second->sendRequestFromPullsQueue();
                requestTimer();
                sendNewRequest = true;
            }
            ++counter;
        }
    }
}

std::ostream& operator<<(std::ostream &os, const Raqsac::SockPair &sp)
{
    os << "locSocket=" << sp.localAddr << ":" << sp.localPort << " " << "remSocket=" << sp.remoteAddr << ":" << sp.remotePort;
    return os;
}

std::ostream& operator<<(std::ostream &os, const RaqsacConnection &conn)
{
    os << "socketId=" << conn.socketId << " ";
    os << "fsmState=" << RaqsacConnection::stateName(conn.getFsmState()) << " ";
    return os;
}

} // namespace raqsac
} // namespace inet

