
#include "DumbRaqsac.h"

#include "../Raqsac.h"

namespace inet {

namespace raqsac {

Register_Class(DumbRaqsac);

#define REXMIT_TIMEOUT    2 // Just a dummy value

DumbRaqsac::DumbRaqsac() :
        RaqsacAlgorithm(), state((DumbRaqsacStateVariables*&) RaqsacAlgorithm::state)
{
    rexmitTimer = nullptr;
}

DumbRaqsac::~DumbRaqsac()
{
    // cancel and delete timers
    if (rexmitTimer)
        delete conn->getRaqsacMain()->cancelEvent(rexmitTimer);
}

void DumbRaqsac::initialize()
{
    RaqsacAlgorithm::initialize();

    rexmitTimer = new cMessage("DumbRaqsac-REXMIT");
    rexmitTimer->setContextPointer(conn);
}

void DumbRaqsac::connectionClosed()
{
    conn->getRaqsacMain()->cancelEvent(rexmitTimer);
}

void DumbRaqsac::processTimer(cMessage *timer, RaqsacEventCode &event)
{
    if (timer != rexmitTimer)
        throw cRuntimeError(timer, "unrecognized timer");
    conn->scheduleTimeout(rexmitTimer, REXMIT_TIMEOUT);
}

void DumbRaqsac::dataSent(uint32 fromseq)
{
    if (rexmitTimer->isScheduled()){
        conn->getRaqsacMain()->cancelEvent(rexmitTimer);
    }
    conn->scheduleTimeout(rexmitTimer, REXMIT_TIMEOUT);
}

} // namespace NDP

} // namespace inet

