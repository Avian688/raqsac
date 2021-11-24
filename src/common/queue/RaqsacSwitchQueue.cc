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
#include "RaqsacSwitchQueue.h"

namespace inet {
namespace queueing {
Define_Module(RaqsacSwitchQueue);

simsignal_t RaqsacSwitchQueue::queueingTimeSignal = registerSignal("queueingTime");
simsignal_t RaqsacSwitchQueue::dataQueueLengthSignal = registerSignal("dataQueueLength");
simsignal_t RaqsacSwitchQueue::headersQueueLengthSignal = registerSignal("headersQueueLength");
simsignal_t RaqsacSwitchQueue::numTrimmedPktSig = registerSignal("numTrimmedPkt");

void RaqsacSwitchQueue::initialize(int stage)
{
    PacketQueueBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        numTrimmedPacketsVec.setName("numTrimmedPacketsVec");
        weight=10;
        dataQueueLength = 0 ;
        headersQueueLength = 0;
        synAckQueueLength=0;
        numTrimmedPkt=0;

        inputGate = gate("in");
        producer = findConnectedModule<IActivePacketSource>(inputGate);
        outputGate = gate("out");
        collector = findConnectedModule<IActivePacketSink>(outputGate);

        subscribe(packetPushedSignal, this);
        subscribe(packetPoppedSignal, this);
        subscribe(packetRemovedSignal, this);
        subscribe(packetDroppedSignal, this);
        subscribe(packetCreatedSignal, this);

        WATCH(dataQueueLength);
        WATCH(headersQueueLength);
        WATCH(synAckQueueLength);
        WATCH(numTrimmedPkt);

        dataQueuePriority1.setName("dataQueuePriority1");
        dataQueuePriority2.setName("dataQueuePriority2");
        dataQueuePriority3.setName("dataQueuePriority3");
        dataQueuePriority4.setName("dataQueuePriority4");
        dataQueueLowestPriority.setName("dataQueueLowestPriority");

        dataQueue.setName("dataQueue");
        headersQueue.setName("headerQueue");
        synAckQueue.setName("synAckQueue");
        // configuration
        packetCapacity = par("packetCapacity");
        // moh added
        recordScalar("packetCapacity= ", packetCapacity);
    }
    else if (stage == INITSTAGE_QUEUEING) {
            checkPushPacketSupport(inputGate);
            checkPopPacketSupport(outputGate);
            if (producer != nullptr)
                producer->handleCanPushPacket(inputGate);
        }
    else if (stage == INITSTAGE_LAST)
        updateDisplayString();
    //emit()
    //statistics
    cSimpleModule::emit(dataQueueLengthSignal, dataQueue.getLength());
    cSimpleModule::emit(headersQueueLengthSignal, headersQueue.getLength());
    cSimpleModule::emit(numTrimmedPktSig, numTrimmedPkt);
}

void RaqsacSwitchQueue::handleMessage(cMessage *message)
{
    auto packet = check_and_cast<Packet *>(message);
    pushPacket(packet, packet->getArrivalGate());
}

bool RaqsacSwitchQueue::isOverloaded() const
{
    return dataQueue.getLength() >= packetCapacity;
}

int RaqsacSwitchQueue::getNumPackets() const
{
    return dataQueue.getLength();
}

Packet *RaqsacSwitchQueue::getPacket(int index) const
{
    if (index < 0 || index >= dataQueue.getLength())
        throw cRuntimeError("index %i out of range", index);
    return check_and_cast<Packet *>(dataQueue.get(index));
}

void RaqsacSwitchQueue::dropPacket(Packet *packet, PacketDropReason reason, int limit)
{
    PacketDropDetails details;
    details.setReason(reason);
    details.setLimit(limit);
    cSimpleModule::emit(packetDroppedSignal, packet, &details);
    delete packet;
}

void RaqsacSwitchQueue::pushPacket(Packet *packet, cGate *gate)
{
    Enter_Method("pushPacket");
    emit(packetPushedSignal, packet);
    EV_INFO << "PACKET STRING" << packet->str() << endl;
    EV_INFO << "Pushing packet " << packet->getName() << " into the queue." << endl;
    const auto& ipv4Datagram = packet->peekAtFront<Ipv4Header>();
    const auto& raqsacHeaderPeek = packet->peekDataAt<raqsac::RaqsacHeader>(ipv4Datagram->getChunkLength());
    if (raqsacHeaderPeek->getSynBit() == true) {
       synAckQueue.insert(packet);
       synAckQueueLength=synAckQueue.getLength();
       return;
    }
    else if (raqsacHeaderPeek->isHeader() == true ) {
        headersQueue.insert(packet);
        headersQueueLength = headersQueue.getLength();
        return;
    }
    else if (raqsacHeaderPeek->isPullPacket() == true ) {
        headersQueue.insert(packet);
        headersQueueLength = headersQueue.getLength();
        return;
    }
    unsigned int priority = raqsacHeaderPeek->getPriorityValue();
    switch (priority) {
    case 0:
        if (getCurrentSharedMemoryLength() >= packetCapacity) {
            trimPacket(packet);
            return;
        } else {
            dataQueueLowestPriority.insert(packet);
            return;
        }
        break;
    case 1:
        if (getCurrentSharedMemoryLength() >= packetCapacity) {
            return;
        } else {
            // dataQueue is not full ==> insert the incoming packet in the dataQueue
            dataQueuePriority1.insert(packet);
            return;
        }
        break;
    case 2:
        if (getCurrentSharedMemoryLength() >= packetCapacity) {
            trimPacket(packet);
            return;
        } else {
            dataQueuePriority2.insert(packet);
            return;
        }
        break;
    case 3:
        if (getCurrentSharedMemoryLength() >= packetCapacity) {
            trimPacket(packet);
            return;
        } else {
            dataQueuePriority3.insert(packet);
            return;
        }
        break;

    case 4:
        if (getCurrentSharedMemoryLength() >= packetCapacity) {
            trimPacket(packet);
            return ;
        } else {
            dataQueuePriority4.insert(packet);
            return;
        }
        break;
    default:
        std::cout << " error priority value!" << endl;
        break;
    }
    return;
}

void RaqsacSwitchQueue::trimPacket(Packet* packet)
{
    std::string header="Header-";
    auto ipv4Header = packet->removeAtFront<Ipv4Header>();
    ASSERT(B(ipv4Header->getTotalLengthField()) >= ipv4Header->getChunkLength());
    if (ipv4Header->getTotalLengthField() < packet->getDataLength())
        packet->setBackOffset(B(ipv4Header->getTotalLengthField()) - ipv4Header->getChunkLength());
    auto raqsacHeader = packet->removeAtFront<raqsac::RaqsacHeader>();
    packet->removeAtFront<GenericAppMsgRaqsac>();
    if (raqsacHeader != nullptr) {
        std::string name=packet->getName();
        std::string rename=header+name;
        packet->setName(rename.c_str());
        raqsacHeader->setIsHeader(true);

        unsigned short srcPort = raqsacHeader->getSrcPort();
        unsigned short destPort = raqsacHeader->getDestPort();
        EV << "NdpSwitchQueue srcPort:" << srcPort << endl;
        EV << "NdpSwitchQueue destPort:" << destPort << endl;
        EV << "NdpSwitchQueue Header Full Name:" << raqsacHeader->getFullName() << endl;
    }
    packet->insertAtFront(raqsacHeader);
    ipv4Header->setTotalLengthField(ipv4Header->getChunkLength() + packet->getDataLength());
    packet->insertAtFront(ipv4Header);
    headersQueue.insert(packet);
    headersQueueLength=headersQueue.getLength();
    ++numTrimmedPkt;
    numTrimmedPacketsVec.record(numTrimmedPkt);
    cSimpleModule::emit(numTrimmedPktSig, numTrimmedPkt);
    return;
}
Packet *RaqsacSwitchQueue::popPacket(cGate *gate) {
    Enter_Method("popPacket");
    if (isEmpty()){
        return nullptr;
    }
    else if (synAckQueue.getLength()!=0){  //syn/ack pop
        auto packet = check_and_cast<Packet *>(synAckQueue.pop());
        cSimpleModule::emit(packetRemovedSignal, packet);
        updateDisplayString();
        animateSend(packet, outputGate);
        return packet;
    }
    else if (headersQueue.getLength() == 0 && getCurrentSharedMemoryLength() == 0) { //dataQueue pop
        return nullptr;
    }
    else if (headersQueue.getLength() == 0 && getCurrentSharedMemoryLength() != 0) { //dataQueue pop
        return popNextPacket();
    }
    else if (headersQueue.getLength() != 0 && getCurrentSharedMemoryLength() == 0) { //dataQueue pop
            return check_and_cast<Packet *>(headersQueue.pop());
        }
    else if ( headersQueue.getLength() != 0 && getCurrentSharedMemoryLength() != 0 && weight%10 == 0) { //round robin dataQueue pop
        auto packet = popNextPacket();
        cSimpleModule::emit(dataQueueLengthSignal, dataQueue.getLength());
        emit(packetRemovedSignal, packet);
        ++weight;
        animateSend(packet, outputGate);
        return packet;
    }
    else if (headersQueue.getLength() != 0 && getCurrentSharedMemoryLength() != 0 ) {
        auto packet = check_and_cast<Packet *>(headersQueue.pop());
        emit(packetRemovedSignal, packet);
        EV_INFO << " get from header queue- size = " << packet->getByteLength() << endl;
        cSimpleModule::emit(headersQueueLengthSignal, headersQueue.getLength());
        ++weight;
        updateDisplayString();
        animateSend(packet, outputGate);
        return packet;
    }
    return nullptr;
}

Packet *RaqsacSwitchQueue::popNextPacket() {
    if (dataQueuePriority1.getLength() != 0) {
        return check_and_cast<Packet *>(dataQueuePriority1.pop());
    } else if (dataQueuePriority2.getLength() != 0) {
        return check_and_cast<Packet *>(dataQueuePriority2.pop());
    } else if (dataQueuePriority3.getLength() != 0) {
        return check_and_cast<Packet *>(dataQueuePriority3.pop());
    } else if (dataQueuePriority4.getLength() != 0) {
        return check_and_cast<Packet *>(dataQueuePriority4.pop());
    } else if (dataQueueLowestPriority.getLength() != 0) {
        return check_and_cast<Packet *>(dataQueueLowestPriority.pop());
    } else {
        std::cout << " none" << endl;
        return nullptr;
    }
}

bool RaqsacSwitchQueue::isEmpty() const {
    if (dataQueueLowestPriority.isEmpty() && headersQueue.isEmpty()
            && synAckQueue.isEmpty() && dataQueuePriority1.isEmpty()
            && dataQueuePriority2.isEmpty() && dataQueuePriority3.isEmpty()
            && dataQueuePriority4.isEmpty()) {
        return true;
    } else {
        return false;
    }
}

void RaqsacSwitchQueue::removePacket(Packet *packet)
{
    Enter_Method("removePacket");
    EV_INFO << "Removing packet " << packet->getName() << " from the queue." << endl;
    dataQueue.remove(packet);
    emit(packetRemovedSignal, packet);
    updateDisplayString();
}

int RaqsacSwitchQueue::getCurrentSharedMemoryLength()
{
    int bufferMemory = dataQueueLowestPriority.getLength() + dataQueuePriority1.getLength() + dataQueuePriority2.getLength() + dataQueuePriority3.getLength() + dataQueuePriority4.getLength();
    return bufferMemory;
}

void RaqsacSwitchQueue::receiveSignal(cComponent *source, simsignal_t signal, cObject *object, cObject *details)
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

void RaqsacSwitchQueue::finish(){
    recordScalar("numTrimmedPkt ",numTrimmedPkt );
    cSimpleModule::emit(numTrimmedPktSig, numTrimmedPkt);
}

}
} // namespace inet
