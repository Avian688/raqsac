#include <string.h>

#include "../contract/raqsac/RaqsacCommand_m.h"
#include "../raqsac/RaqsacAlgorithm.h"
#include "../raqsac/RaqsacConnection.h"
#include "raqsac_common/RaqsacHeader.h"
#include "Raqsac.h"

namespace inet {

namespace raqsac {

//
// Event processing code
//
void RaqsacConnection::process_OPEN_ACTIVE(RaqsacEventCode &event, RaqsacCommand *RaqsacCommand, cMessage *msg)
{
    RaqsacOpenCommand *openCmd = check_and_cast<RaqsacOpenCommand*>(RaqsacCommand);
    L3Address localAddr, remoteAddr;
    int localPort, remotePort;
    switch (fsm.getState()) {
        case RAQSAC_S_INIT:
            initConnection(openCmd);
            // store local/remote socket
            state->active = true;
            localAddr = openCmd->getLocalAddr();
            remoteAddr = openCmd->getRemoteAddr();
            localPort = openCmd->getLocalPort();
            remotePort = openCmd->getRemotePort();
            state->numSymbolsToSend = openCmd->getNumSymbolsToSend();
            state->priorityValue = openCmd->getPriorityValue();
            if (remoteAddr.isUnspecified() || remotePort == -1){
                throw cRuntimeError(raqsacMain, "Error processing command OPEN_ACTIVE: remote address and port must be specified");
            }
            if (localPort == -1) {
                localPort = raqsacMain->getEphemeralPort();
            }
            EV_DETAIL << "process_OPEN_ACTIVE OPEN: " << localAddr << ":" << localPort << " --> " << remoteAddr << ":" << remotePort << endl;
            raqsacMain->addSockPair(this, localAddr, remoteAddr, localPort, remotePort);
            sendEstabIndicationToApp();
            sendInitialWindow();
            break;
        default:
            throw cRuntimeError(raqsacMain, "Error processing command OPEN_ACTIVE: connection already exists");
    }
    delete openCmd;
    delete msg;
}

void RaqsacConnection::process_OPEN_PASSIVE(RaqsacEventCode &event, RaqsacCommand *RaqsacCommand, cMessage *msg)
{
    RaqsacOpenCommand *openCmd = check_and_cast<RaqsacOpenCommand*>(RaqsacCommand);
    L3Address localAddr;
    int localPort;
    switch (fsm.getState()) {
        case RAQSAC_S_INIT:
            initConnection(openCmd);
            state->active = false;
            localAddr = openCmd->getLocalAddr();
            localPort = openCmd->getLocalPort();
            if (localPort == -1)
                throw cRuntimeError(raqsacMain, "Error processing command OPEN_PASSIVE: local port must be specified");
            raqsacMain->addSockPair(this, localAddr, L3Address(), localPort, -1);
            break;
        default:
            throw cRuntimeError(raqsacMain, "Error processing command OPEN_PASSIVE: connection already exists");
    }
    delete openCmd;
    delete msg;
}

} // namespace ndp

} // namespace inet

