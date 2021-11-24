#include <string.h>

#include <inet/applications/common/SocketTag_m.h>
#include <inet/common/TimeTag_m.h>
#include <random>
#include "../../application/raqsacapp/GenericAppMsgRaqsac_m.h"
#include "../contract/raqsac/RaqsacCommand_m.h"
#include "../raqsac/RaqsacAlgorithm.h"
#include "../raqsac/RaqsacConnection.h"
#include "raqsac_common/RaqsacHeader.h"
#include "Raqsac.h"

namespace inet {
namespace raqsac {

void RaqsacConnection::pushSymbol()
{
    EV_TRACE << "RaqsacConnection::pushSymbol()" << endl;
    int ini = state->esi + 1;
    std::string packetName = "SYMBOL-" + std::to_string(ini);
    Packet *packet = new Packet(packetName.c_str());
    const auto &payload = makeShared<GenericAppMsgRaqsac>();
    payload->setChunkLength(B(1453));
    packet->insertAtBack(payload);
    EncodingSymbols encodingSymbols;
    encodingSymbols.ESI = ini;
    encodingSymbols.msg = packet;
    encodingSymbolsList.push_back(encodingSymbols);
}

void RaqsacConnection::sendInitialWindow()
{

    // TODO  we don't do any checking about the received request segment, e.g. check if it's  a request nothing else
    // fetch the next Packet from the encodingPackets list
    EV_TRACE << "RaqsacConnection::sendInitialWindow";
    ASSERT(state->esi == 0);
    std::list<EncodingSymbols>::iterator itt;
        if (state->IW > state->numSymbolsToSend) {
            state->IW = state->numSymbolsToSend;
        }
        for (int i = 1; i <= state->IW; i++) {
            pushSymbol();
            itt = encodingSymbolsList.begin();
            std::advance(itt, state->esi);

            const auto &raqsacseg = makeShared<RaqsacHeader>();
            Packet *fp = itt->msg->dup();
            state->esi++;
            state->request_id = 0;
            raqsacseg->setESI(state->esi);
            raqsacseg->setSBN(state->sbn);  //not used at the moment
            raqsacseg->setPriorityValue(state->priorityValue);
            raqsacseg->setIsPullPacket(false);
            raqsacseg->setIsHeader(false);
            raqsacseg->addTag<CreationTimeTag>()->setCreationTime(simTime());

            EV_INFO << "Sending IW symbol " << raqsacseg->getESI() << endl;
            raqsacseg->setSynBit(true);
            raqsacseg->setNumSymbolsToSend(state->numSymbolsToSend);
            sendToIP(fp, raqsacseg);
        }
}

RaqsacEventCode RaqsacConnection::process_RCV_SEGMENT(Packet *packet, const Ptr<const RaqsacHeader> &raqsacseg, L3Address src, L3Address dest)
{
    EV_TRACE << "RaqsacConnection::process_RCV_SEGMENT" << endl;
    //EV_INFO << "Seg arrived: ";
    //printSegmentBrief(packet, raqsacseg);
    EV_DETAIL << "TCB: " << state->str() << "\n";
    RaqsacEventCode event;
    if (fsm.getState() == RAQSAC_S_LISTEN) {
        EV_INFO << "RAQSAC_S_LISTEN processing the segment in listen state" << endl;
        event = processSegmentInListen(packet, raqsacseg, src, dest);
        if (event == RAQSAC_E_RCV_SYN) {
            EV_INFO << "RAQSAC_E_RCV_SYN received syn. Changing state to Established" << endl;
            FSM_Goto(fsm, RAQSAC_S_ESTABLISHED);
            EV_INFO << "Processing Segment" << endl;
            event = processSegment1stThru8th(packet, raqsacseg);
        }
    }
    else {
        raqsacMain->updateSockPair(this, dest, src, raqsacseg->getDestPort(), raqsacseg->getSrcPort());
        event = processSegment1stThru8th(packet, raqsacseg);
    }
    delete packet;
    return event;
}

RaqsacEventCode RaqsacConnection::processSegment1stThru8th(Packet *packet, const Ptr<const RaqsacHeader> &raqsacseg)
{
    EV_TRACE << "RaqsacConnection::processSegment1stThru8th" << endl;
    EV_INFO << "_________________________________________" << endl;
    RaqsacEventCode event = RAQSAC_E_IGNORE;

    // (S.3)  at the sender: PULL pkt arrived, this pkt triggers either retransmission of trimmed pkt or sending a new data pkt.
    // ££££££££££££££££££££££££££££££££££££££££££££££££££££££££££££££££££££££
    // ££££££££££££££££££££££££ REQUEST Arrived at the sender £££££££££££££££
    // ££££££££££££££££££££££££££££££££££££££££££££££££££££££££££££££££££££££
    ASSERT(fsm.getState() == RAQSAC_S_ESTABLISHED);
    if (raqsacseg->isPullPacket() == true) {
        int requestsGap = raqsacseg->getPullSequenceNumber() - state->request_id;
        std::list<EncodingSymbols>::iterator itt;
        EV_INFO << "Pull packet arrived at the sender - request gap " << requestsGap << endl;
        if (requestsGap >= 1) {
            //  we send Packets  based on requestsGap value
            // if the requestsGap is smaller than 1 that means we received a delayed request which we need to  ignore
            // as we have assumed it was lost and we send extra Packets previously
            for (int i = 1; i <= requestsGap; i++) {
                pushSymbol();
                state->esi = encodingSymbolsList.size() - 1;
                itt = encodingSymbolsList.begin();
                std::advance(itt, state->esi);

                const auto &raqsacseg = makeShared<RaqsacHeader>();
                Packet *fp = itt->msg->dup();
                state->esi++;
                ++state->request_id;
                raqsacseg->setESI(state->esi);
                raqsacseg->setSBN(state->sbn);  //not used at the moment
                raqsacseg->setPriorityValue(state->priorityValue);
                raqsacseg->setIsPullPacket(false);
                raqsacseg->setIsHeader(false);
                raqsacseg->setNumSymbolsToSend(state->numSymbolsToSend);
                raqsacseg->addTag<CreationTimeTag>()->setCreationTime(simTime());
                EV_INFO << "Sending data packet - " << raqsacseg->getESI() << endl;
                raqsacseg->setNumSymbolsToSend(state->numSymbolsToSend);
                sendToIP(fp, raqsacseg);
            }
        }
        else if (requestsGap < 1) {
            EV_INFO << "Delayed pull arrived --> ignore it" << endl;
        }
    }
    // (R.1)  at the receiver
    // $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
    // $$$$$$$$$$$$$$$$$$$$$$  HEADER arrived   $$$$$$$$$$$$$$$$   Rx
    // $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
    // header arrived at the receiver==> send new request with pacing (fixed pacing: MTU/1Gbps)
    else if (raqsacseg->isHeader() == true) { // 1 read, 2 write
        EV_INFO << "Header arrived at the receiver" << endl;
        state->numRcvTrimmedHeader++;
        addRequestToPullsQueue();
        if (state->numberReceivedPackets == 0 && state->connNotAddedYet == true) {
            getRaqsacMain()->requestCONNMap[getRaqsacMain()->connIndex] = this; // moh added
            state->connNotAddedYet = false;
            ++getRaqsacMain()->connIndex;
            EV_INFO << "sending first request" << endl;
            getRaqsacMain()->sendFirstRequest();
        }
    }

    // (R.2) at the receiver
    // $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
    // $$$$$$$$$$$$$$$$$$$$$$  symbol arrived at the receiver  $$$$$$$$$$$$$$$$
    // $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
    else{
        EV_INFO << "Data packet arrived at the receiver - seq num " << raqsacseg->getESI() << endl;
        state->esi++;
        state->numberReceivedPackets++;
        int numberReceivedSymbols = state->numberReceivedPackets;
        int arrivedSymbolsInternalIndex = state->esi;
        int arrivedSymbolsActualIndex = raqsacseg->getESI();
        int arrivedSymbolsGap = arrivedSymbolsActualIndex - arrivedSymbolsInternalIndex;

        int initialSentSymbols = state->IW;
        int wantedSymbols = 0;
        if (state->numRcvTrimmedHeader == 0) {
            wantedSymbols = state->numPacketsToGet;
        }
        else if (state->numRcvTrimmedHeader > 0) {
            wantedSymbols = state->numPacketsToGet + 2; //Overhead
        }
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%% 1   %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        if (numberReceivedSymbols > wantedSymbols && state->redoDecoding == false) {
            EV_INFO << "All packets received - finish all connections!" << endl;
            state->connFinished = true;
            getRaqsacMain()->allConnFinished();
            goto ll;
        }
        else {
            if (numberReceivedSymbols < wantedSymbols && state->connFinished == false) {
                if (arrivedSymbolsGap == 0 && numberReceivedSymbols <= (wantedSymbols - initialSentSymbols)) { //
                    addRequestToPullsQueue();
                }
                else if (arrivedSymbolsGap > 0 && numberReceivedSymbols <= (wantedSymbols - initialSentSymbols)) { //
                    int h = 0;
                    addRequestToPullsQueue();
                    arrivedSymbolsGap--;
                    h++;
                    state->esi = arrivedSymbolsActualIndex;
                }
                else if (arrivedSymbolsGap < 0) {
                    --state->esi; // to be used in case of receiving a delayed symbol
                    addRequestToPullsQueue();
                }
                else if (arrivedSymbolsGap > 0 && numberReceivedSymbols > (wantedSymbols - initialSentSymbols)) { //numberReceivedSymbols > numberSentRequests
                    int h = 0;
                    addRequestToPullsQueue();
                    arrivedSymbolsGap--;
                    h++;
                    if (numberReceivedSymbols == wantedSymbols - 1)
                        arrivedSymbolsGap = 0; // just send one request (this will ends the while loop, but we already sent a new reques) even the gap is larger than 1, as we received  this number of symbols: wantedSymbols-1
                    state->esi = arrivedSymbolsActualIndex;
                }
            }
            if (numberReceivedSymbols == 1 && state->connNotAddedYet == true) {
                getRaqsacMain()->requestCONNMap[getRaqsacMain()->connIndex] = this; // moh added
                ++getRaqsacMain()->connIndex;
                state->connNotAddedYet = false;
                EV << "Requesting Pull Timer" << endl;
                getRaqsacMain()->sendFirstRequest();
            }
            // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
            // %%%%%%%%%%%%%%%%%%%%%%%%%  4  %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
            // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
            //  send any received Packet to the app
            auto tag = raqsacseg->getTag(0);
            const CreationTimeTag *timeTag = dynamic_cast<const CreationTimeTag *>(tag);
            //packet->setFrontOffset(B(0));
            //packet->setBackOffset(B(1500));
            Ptr<Chunk> msgRx;
            msgRx = packet->removeAll();
            if (state->connFinished == false) {
                EV_INFO << "Sending Data Packet to Application" << endl;
                // buffer the received Packet segment
                std::list<EncodingSymbols>::iterator itR;  // received iterator
                itR = receivedSymbolsList.begin();
                std::advance(itR, state->esi); // increment the iterator by esi
                // MOH: Send any received Packet to the app, just for now to test the Incast example, this shouldn't be the normal case
                //Packet *newPacket = packet->dup();
                std::string packetName = "SYMBOL-" + std::to_string(state->esi);
                Packet *newPacket = new Packet(packetName.c_str(), msgRx);
                newPacket->addTag<CreationTimeTag>()->setCreationTime(timeTag->getCreationTime());
                EncodingSymbols receivedEncodingSymbols;
                receivedEncodingSymbols.ESI = state->esi;
                receivedEncodingSymbols.msg = newPacket;
                receivedSymbolsList.push_back(receivedEncodingSymbols);
                newPacket->setKind(RAQSAC_I_DATA); // TBD currently we never send NDP_I_URGENT_DATA
                newPacket->addTag<SocketInd>()->setSocketId(socketId);
                EV_INFO << "Sending to App packet: " << newPacket->str() << endl;
                sendToApp(newPacket);
            }
            // All the Packets have been received
            if (state->isfinalReceivedPrintedOut == false) {
                EV_INFO << "Total Received Symbols: " << numberReceivedSymbols << endl;
                EV_INFO << "Total Wanted Symbols: " << wantedSymbols << endl;
                if (numberReceivedSymbols == wantedSymbols || state->connFinished == true || state->redoDecoding == true) {
                    if(isDecoderSuccessful() || state->redoDecoding == true){
                        std::list<EncodingSymbols>::iterator iter; // received iterator
                        iter = receivedSymbolsList.begin();
                        int index = 0;
                        while (iter != receivedSymbolsList.end()) {
                            iter++;
                        }
                        EV_INFO << " numRcvTrimmedHeader:    " << state->numRcvTrimmedHeader << endl;
                        EV_INFO << "CONNECTION FINISHED!" << endl;
                        sendIndicationToApp(RAQSAC_I_PEER_CLOSED); // this is ok if the sinkApp is used by one conn
                        state->isfinalReceivedPrintedOut = true;
                    }
                    else{
                        addRequestToPullsQueue();
                        state->redoDecoding = true;
                    }
                }
            }
        }
    }
    ll: return event;
}

bool RaqsacConnection::isDecoderSuccessful()
{
    if (state->numRcvTrimmedHeader == 0) {
        EV_INFO << "No Decoding Required!" << endl;
        return true; // no need for decoding (RaQSac is systematic)
    }
    std::exponential_distribution<double> expDist = std::exponential_distribution<double>(2);
    std::mt19937 PRNG = std::mt19937(1111);
    double expDistVal = expDist.operator()(PRNG);
    std::cout << " number = " << expDist << std::endl;
    if (expDistVal >= 6.907) {
        state->numTimesDecodingFailed++;
        EV_INFO << "Decoding failed!" << endl;
        //  if decoding fails send new request
        return false;
    }
    std::cout << " We need decoding and it Succeeded :)) \n";
    return true;
}

void RaqsacConnection::addRequestToPullsQueue()
{
    EV_TRACE << "RaqsacConnection::addRequestToPullsQueue" << endl;
    ++state->request_id;
    char msgname[16];
    sprintf(msgname, "PULL-%d", state->request_id);
    Packet *raqsacpack = new Packet(msgname);

    const auto &raqsacseg = makeShared<RaqsacHeader>();
    raqsacseg->setIsPullPacket(true);
    raqsacseg->setIsHeader(false);
    raqsacseg->setSynBit(false);
    raqsacseg->setPullSequenceNumber(state->request_id);
    raqsacpack->insertAtFront(raqsacseg);
    pullQueue.insert(raqsacpack);
    EV_INFO << "Adding new request to the pull queue -- pullsQueue length now = " << pullQueue.getLength() << endl;
    bool napState = getRaqsacMain()->getNapState();
    if (napState == true) {
        EV_INFO << "Requesting Pull Timer (8 microseconds)" << endl;
        getRaqsacMain()->requestTimer();
    }
}

void RaqsacConnection::sendRequestFromPullsQueue()
{
    EV_TRACE << "RaqsacConnection::sendRequestFromPullsQueue" << endl;
    if (pullQueue.getLength() > 0) {
        Packet *fp = check_and_cast<Packet*>(pullQueue.pop());
        auto raqsacseg = fp->removeAtFront<raqsac::RaqsacHeader>();
        EV << "a request has been popped from the Pull queue, the new queue length  = " << pullQueue.getLength() << " \n\n";
        sendToIP(fp, raqsacseg);
    }
}

int RaqsacConnection::getPullsQueueLength()
{
    int len = pullQueue.getLength();
    return len;
}

bool RaqsacConnection::isConnFinished()
{
    return state->connFinished;
}

int RaqsacConnection::getNumRcvdPackets()
{
    return state->numberReceivedPackets;
}

void RaqsacConnection::setConnFinished()
{
    state->connFinished = true;
}

RaqsacEventCode RaqsacConnection::processSegmentInListen(Packet *packet, const Ptr<const RaqsacHeader> &raqsacseg, L3Address srcAddr, L3Address destAddr)
{
    EV_DETAIL << "Processing segment in LISTEN" << endl;

    if (raqsacseg->getSynBit()) {
        EV_DETAIL << "SYN bit set: filling in foreign socket" << endl;
        raqsacMain->updateSockPair(this, destAddr, srcAddr, raqsacseg->getDestPort(), raqsacseg->getSrcPort());
        // this is a receiver
        state->numPacketsToGet = raqsacseg->getNumSymbolsToSend();
        return RAQSAC_E_RCV_SYN; // this will take us to SYN_RCVD
    }
    EV_WARN << "Unexpected segment: dropping it" << endl;
    return RAQSAC_E_IGNORE;
}

void RaqsacConnection::segmentArrivalWhileClosed(Packet *packet, const Ptr<const RaqsacHeader> &raqsacseg, L3Address srcAddr, L3Address destAddr)
{
    EV_TRACE << "RaqsacConnection::segmentArrivalWhileClosed" << endl;
    EV_INFO << "Seg arrived: " << endl;
    printSegmentBrief(packet, raqsacseg);
    // This segment doesn't belong to any connection, so this object
    // must be a temp object created solely for the purpose of calling us
    ASSERT(state == nullptr);
    EV_INFO << "Segment doesn't belong to any existing connection" << endl;
    EV_FATAL << "RaqsacConnection::segmentArrivalWhileClosed should not be called!";
}

}    // namespace ndp

} // namespace inet

