//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#include <inet/common/INETDefs.h>
#include <inet/common/ModuleAccess.h>
#include <inet/common/Simsignals.h>
#include <inet/queueing/function/PacketComparatorFunction.h>
#include <inet/queueing/function/PacketDropperFunction.h>
#include <inet/queueing/base/PacketQueueBase.h>

#include "../../application/raqsacapp/GenericAppMsgRaqsac_m.h"
#include "RaqsacSwitchQueueController.h"

namespace inet {
namespace queueing {
Define_Module(RaqsacSwitchQueueController);

simsignal_t RaqsacSwitchQueueController::queueingTimeSignal = registerSignal("queueingTime");
simsignal_t RaqsacSwitchQueueController::dataQueueLengthSignal = registerSignal("dataQueueLength");
simsignal_t RaqsacSwitchQueueController::headersQueueLengthSignal = registerSignal("headersQueueLength");
simsignal_t RaqsacSwitchQueueController::numTrimmedPktSig = registerSignal("numTrimmedPkt");

void RaqsacSwitchQueueController::initialize(int stage)
{
    PacketQueueBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        numTrimmedPacketsVec.setName("numTrimmedPacketsVec");
        numTrimmedPkt=0;

        subscribe(packetPushedSignal, this);
        subscribe(packetPoppedSignal, this);
        subscribe(packetRemovedSignal, this);
        subscribe(packetDroppedSignal, this);
        subscribe(packetCreatedSignal, this);
        WATCH(numTrimmedPkt);
        packetCapacity = par("packetCapacity");
        recordScalar("packetCapacity= ", packetCapacity);
    }
    else if (stage == INITSTAGE_LAST)
        updateDisplayString();
    cSimpleModule::emit(numTrimmedPktSig, numTrimmedPkt);
}

void RaqsacSwitchQueueController::handleMessage(cMessage *message)
{
    auto packet = check_and_cast<Packet *>(message);
    pushPacket(packet, packet->getArrivalGate());
}

int RaqsacSwitchQueueController::getNumPackets() const
{
    return 0;
}

Packet *RaqsacSwitchQueueController::getPacket(int index) const
{
    return nullptr;
}

void RaqsacSwitchQueueController::dropPacket(Packet *packet, PacketDropReason reason, int limit)
{
    PacketDropDetails details;
    details.setReason(reason);
    details.setLimit(limit);
    cSimpleModule::emit(packetDroppedSignal, packet, &details);
    delete packet;
}

void RaqsacSwitchQueueController::pushPacket(Packet *packet, cGate *gate)
{
    Enter_Method("pushPacket");
    emit(packetPushedSignal, packet);
    EV_INFO << "PACKET STRING" << packet->str() << endl;
    EV_INFO << "Pushing packet " << packet->getName() << " into the queue." << endl;
    const auto& ipv4Datagram = packet->peekAtFront<Ipv4Header>();
    const auto& raqsacHeaderPeek = packet->peekDataAt<raqsac::RaqsacHeader>(ipv4Datagram->getChunkLength());
    std::hash<std::string> hash;
    std::string key = ipv4Datagram->getSrcAddress().str() + ipv4Datagram->getDestAddress().str() + std::to_string(raqsacHeaderPeek->getSrcPort())+ std::to_string(raqsacHeaderPeek->getDestPort());
    if(connectionQueues.find(key) == connectionQueues.end()){
        RaqsacSwitchQueue *switchQueue = new RaqsacSwitchQueue();
        switchQueue->pushPacket(packet, gate);
        connectionQueues[key] = switchQueue;
    }
    else{
        RaqsacSwitchQueue *switchQueue = connectionQueues.at(key);
        switchQueue->pushPacket(packet, gate);
    }
}

Packet *RaqsacSwitchQueueController::popPacket(cGate *gate) {
    Enter_Method("popPacket");
//    if (isEmpty()){
//        return nullptr;
//    }
//    else{
//        return connectionQueues.begin()->second->popPacket(gate);
//    }
    for(auto &q:connectionQueues){
        RaqsacSwitchQueue *queue = q.second;
        if(queue){
            if(!queue->isEmpty()){
                return queue->popPacket(gate);
            }
        }
    }

}

bool RaqsacSwitchQueueController::isEmpty() const {
    bool isEmpty = true;
    for(auto &q:connectionQueues){
        RaqsacSwitchQueue *queue = q.second;
        if(queue){
            if(!queue->isEmpty()){
                isEmpty = false;
            }
        }
    }
    return isEmpty;
}

void RaqsacSwitchQueueController::receiveSignal(cComponent *source, simsignal_t signal, cObject *object, cObject *details)
{
    Enter_Method("receiveSignal");
    if (signal == packetPushedSignal || signal == packetPoppedSignal || signal == packetRemovedSignal)
        ;
    else if (signal == packetDroppedSignal)
        numDroppedPackets++;
    else if (signal == packetCreatedSignal)
        numCreatedPackets++;
    else
        throw cRuntimeError("Unknown signal");
    updateDisplayString();
}

void RaqsacSwitchQueueController::finish(){
    recordScalar("numTrimmedPkt ",numTrimmedPkt );
    cSimpleModule::emit(numTrimmedPktSig, numTrimmedPkt);
}

}
} // namespace inet
