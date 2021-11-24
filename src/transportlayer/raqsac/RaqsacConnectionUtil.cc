#include <string.h>
#include <algorithm>
#include <inet/networklayer/contract/IL3AddressType.h>
#include <inet/networklayer/common/IpProtocolId_m.h>
#include <inet/applications/common/SocketTag_m.h>
#include <inet/common/INETUtils.h>
#include <inet/common/packet/Message.h>
#include <inet/networklayer/common/EcnTag_m.h>
#include <inet/networklayer/common/IpProtocolId_m.h>
#include <inet/networklayer/common/L3AddressResolver.h>
#include <inet/networklayer/common/L3AddressTag_m.h>
#include <inet/networklayer/common/HopLimitTag_m.h>
#include <inet/common/Protocol.h>
#include <inet/common/TimeTag_m.h>
#include "../../application/raqsacapp/GenericAppMsgRaqsac_m.h"

#include "../common/L4ToolsRaqsac.h"
#include "../contract/raqsac/RaqsacCommand_m.h"
#include "../raqsac/RaqsacAlgorithm.h"
#include "../raqsac/RaqsacConnection.h"
#include "raqsac_common/RaqsacHeader.h"
#include "Raqsac.h"
namespace inet {

namespace raqsac {

//
// helper functions
//

const char* RaqsacConnection::stateName(int state)
{
#define CASE(x)    case x: \
        s = #x + 5; break
    const char *s = "unknown";
    switch (state) {
        CASE(RAQSAC_S_INIT)
;            CASE(RAQSAC_S_CLOSED);
            CASE(RAQSAC_S_LISTEN);
            CASE(RAQSAC_S_ESTABLISHED);
        }
    return s;
#undef CASE
}

const char* RaqsacConnection::eventName(int event)
{
#define CASE(x)    case x: \
        s = #x + 5; break
    const char *s = "unknown";
    switch (event) {
        CASE(RAQSAC_E_IGNORE)
;            CASE(RAQSAC_E_OPEN_ACTIVE);
            CASE(RAQSAC_E_OPEN_PASSIVE);
        }
    return s;
#undef CASE
}

const char* RaqsacConnection::indicationName(int code)
{
#define CASE(x)    case x: \
        s = #x + 5; break
    const char *s = "unknown";
    switch (code) {
        CASE(RAQSAC_I_DATA);
        CASE(RAQSAC_I_ESTABLISHED);
        CASE(RAQSAC_I_PEER_CLOSED);

        }
    return s;
#undef CASE
}

void RaqsacConnection::sendToIP(Packet *packet, const Ptr<RaqsacHeader> &raqsacseg)
{
    EV_TRACE << "RaqsacConnection::sendToIP" << endl;
    raqsacseg->setSrcPort(localPort);
    raqsacseg->setDestPort(remotePort);
    //EV_INFO << "Sending: " << endl;
    //printSegmentBrief(packet, raqsacseg);
    IL3AddressType *addressType = remoteAddr.getAddressType();
    packet->addTagIfAbsent<DispatchProtocolReq>()->setProtocol(addressType->getNetworkProtocol());
    auto addresses = packet->addTagIfAbsent<L3AddressReq>();
    addresses->setSrcAddress(localAddr);
    addresses->setDestAddress(remoteAddr);
    insertTransportProtocolHeader(packet, Protocol::raqsac, raqsacseg);
    raqsacMain->sendFromConn(packet, "ipOut");
}

void RaqsacConnection::sendToIP(Packet *packet, const Ptr<RaqsacHeader> &raqsacseg, L3Address src, L3Address dest)
{
    //EV_INFO << "Sending: ";
    //printSegmentBrief(packet, raqsacseg);
    IL3AddressType *addressType = dest.getAddressType();
    packet->addTagIfAbsent<DispatchProtocolReq>()->setProtocol(addressType->getNetworkProtocol());
    auto addresses = packet->addTagIfAbsent<L3AddressReq>();
    addresses->setSrcAddress(src);
    addresses->setDestAddress(dest);

    insertTransportProtocolHeader(packet, Protocol::raqsac, raqsacseg);
    raqsacMain->sendFromConn(packet, "ipOut");
}

void RaqsacConnection::sendIndicationToApp(int code, const int id)
{
    EV_INFO << "Notifying app: " << indicationName(code) << endl;
    auto indication = new Indication(indicationName(code), code);
    RaqsacCommand *ind = new RaqsacCommand();
    ind->setNumRcvTrimmedHeader(state->numRcvTrimmedHeader);
    ind->setNumTimesDecodingFailed(state->numTimesDecodingFailed);
    ind->setUserId(id);
    indication->addTag<SocketInd>()->setSocketId(socketId);
    indication->setControlInfo(ind);
    sendToApp(indication);
}

void RaqsacConnection::sendEstabIndicationToApp()
{
    EV_INFO << "Notifying app: " << indicationName(RAQSAC_I_ESTABLISHED) << endl;
    auto indication = new Indication(indicationName(RAQSAC_I_ESTABLISHED), RAQSAC_I_ESTABLISHED);
    RaqsacConnectInfo *ind = new RaqsacConnectInfo();
    ind->setLocalAddr(localAddr);
    ind->setRemoteAddr(remoteAddr);
    ind->setLocalPort(localPort);
    ind->setRemotePort(remotePort);
    indication->addTag<SocketInd>()->setSocketId(socketId);
    indication->setControlInfo(ind);
    sendToApp(indication);
}

void RaqsacConnection::sendToApp(cMessage *msg)
{
    raqsacMain->sendFromConn(msg, "appOut");
}

void RaqsacConnection::initConnection(RaqsacOpenCommand *openCmd)
{
    //create algorithm
    const char *raqsacAlgorithmClass = openCmd->getRaqsacAlgorithmClass();

    if (!raqsacAlgorithmClass || !raqsacAlgorithmClass[0])
        raqsacAlgorithmClass = raqsacMain->par("raqsacAlgorithmClass");

    raqsacAlgorithm = check_and_cast<RaqsacAlgorithm*>(inet::utils::createOne(raqsacAlgorithmClass));
    raqsacAlgorithm->setConnection(this);
    // create state block
    state = raqsacAlgorithm->getStateVariables();
    configureStateVariables();
    raqsacAlgorithm->initialize();
}

void RaqsacConnection::configureStateVariables()
{
    state->IW = raqsacMain->par("initialWindow");
    raqsacMain->recordScalar("initialWindow=", state->IW);

}

void RaqsacConnection::printConnBrief() const
{
    EV_DETAIL << "Connection " << localAddr << ":" << localPort << " to " << remoteAddr << ":" << remotePort << "  on socketId=" << socketId << "  in " << stateName(fsm.getState()) << endl;
}

void RaqsacConnection::printSegmentBrief(Packet *packet, const Ptr<const RaqsacHeader> &raqsacseg)
{
    EV_STATICCONTEXT
    ;
    EV_INFO << "." << raqsacseg->getSrcPort() << " > ";
    EV_INFO << "." << raqsacseg->getDestPort() << ": ";
    EV_INFO << endl;
}

uint32 RaqsacConnection::convertSimtimeToTS(simtime_t simtime)
{
    ASSERT(SimTime::getScaleExp() <= -3);
    uint32 timestamp = (uint32) (simtime.inUnit(SIMTIME_MS));
    return timestamp;
}

simtime_t RaqsacConnection::convertTSToSimtime(uint32 timestamp)
{
    ASSERT(SimTime::getScaleExp() <= -3);
    simtime_t simtime(timestamp, SIMTIME_MS);
    return simtime;
}

} // namespace ndp

} // namespace inet

