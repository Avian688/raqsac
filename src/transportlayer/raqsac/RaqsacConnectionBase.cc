#include <string.h>
#include <assert.h>

#include "RaqsacAlgorithm.h"
#include "RaqsacConnection.h"
#include "raqsac_common/RaqsacHeader.h"
#include "Raqsac.h"

using namespace std;
namespace inet {

namespace raqsac {
Define_Module(RaqsacConnection);

RaqsacStateVariables::RaqsacStateVariables()
{
    internal_request_id = 0;
    request_id = 0;  // source block number (8-bit unsigned integer)

    numPacketsToGet = 0;
    numSymbolsToSend = 0;
    esi = 0;  // encoding symbol ID
    sbn = 0; // Source block number (8-bit unsigned integer)
    numRcvTrimmedHeader = 0;
    numTimesDecodingFailed = 0;
    numberReceivedPackets = 0;
    numberSentPackets = 0;
    IW = 0; // send the initial window (12 Packets as in NDP) IWWWWWWWWWWWW
    connFinished = false;
    isfinalReceivedPrintedOut = false;
    numRcvdPkt = 0;
    connNotAddedYet = true;
    redoDecoding = false;
    active = false;
}

std::string RaqsacStateVariables::str() const
{
    std::stringstream out;
    return out.str();
}

std::string RaqsacStateVariables::detailedInfo() const
{
    std::stringstream out;
    out << "active=" << active << "\n";
    return out.str();
}

void RaqsacConnection::initConnection(Raqsac *_mod, int _socketId)
{
    Enter_Method_Silent
    ();

    raqsacMain = _mod;
    socketId = _socketId;

    fsm.setName(getName());
    fsm.setState(RAQSAC_S_INIT);

    // queues and algorithm will be created on active or passive open
}

RaqsacConnection::~RaqsacConnection()
{

    std::list<EncodingSymbols>::iterator iter;  // received iterator
    iter = encodingSymbolsList.begin();
    while (!encodingSymbolsList.empty()) {
        delete encodingSymbolsList.front().msg;
        iter++;
        encodingSymbolsList.pop_front();
    }
    while (!receivedSymbolsList.empty()) {
        delete receivedSymbolsList.front().msg;
        receivedSymbolsList.pop_front();
    }
    delete raqsacAlgorithm;
    delete state;
}

void RaqsacConnection::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        if (!processTimer(msg))
            EV_INFO << "\nConnection Attempted Removal!\n";
    }
    else
        throw cRuntimeError("model error: RaqsacConnection allows only self messages");
}

bool RaqsacConnection::processTimer(cMessage *msg)
{
    printConnBrief();
    EV_DETAIL << msg->getName() << " timer expired\n";

    // first do actions
    RaqsacEventCode event;
    event = RAQSAC_E_IGNORE;
    raqsacAlgorithm->processTimer(msg, event); // seeeee processTimer method in NDPBaseAlg.cc
    // then state transitions
    return performStateTransition(event);
}

bool RaqsacConnection::processRaqsacSegment(Packet *packet, const Ptr<const RaqsacHeader> &raqsacseg, L3Address segSrcAddr, L3Address segDestAddr)
{
    Enter_Method_Silent
    ();

    printConnBrief();
    RaqsacEventCode event = process_RCV_SEGMENT(packet, raqsacseg, segSrcAddr, segDestAddr);
    // then state transitions
    return performStateTransition(event);
}

bool RaqsacConnection::processAppCommand(cMessage *msg)
{
    Enter_Method_Silent
    ();

    printConnBrief();

    RaqsacCommand *raqsacCommand = check_and_cast_nullable<RaqsacCommand*>(msg->removeControlInfo());
    RaqsacEventCode event = preanalyseAppCommandEvent(msg->getKind());
    EV_INFO << "App command eventName: " << eventName(event) << "\n";
    switch (event) {
        case RAQSAC_E_OPEN_ACTIVE:
            process_OPEN_ACTIVE(event, raqsacCommand, msg);
            break;

        case RAQSAC_E_OPEN_PASSIVE:
            process_OPEN_PASSIVE(event, raqsacCommand, msg);
            break;

        default:
            throw cRuntimeError(raqsacMain, "wrong event code");
    }

    // then state transitions
    return performStateTransition(event);
}

RaqsacEventCode RaqsacConnection::preanalyseAppCommandEvent(int commandCode)
{
    switch (commandCode) {
        case RAQSAC_C_OPEN_ACTIVE:
            return RAQSAC_E_OPEN_ACTIVE;

        case RAQSAC_C_OPEN_PASSIVE:
            return RAQSAC_E_OPEN_PASSIVE;

        default:
            throw cRuntimeError(raqsacMain, "Unknown message kind in app command");
    }
}

bool RaqsacConnection::performStateTransition(const RaqsacEventCode &event)
{
    ASSERT(fsm.getState() != RAQSAC_S_CLOSED); // closed connections should be deleted immediately

    if (event == RAQSAC_E_IGNORE) {    // e.g. discarded segment
        EV_DETAIL << "Staying in state: " << stateName(fsm.getState()) << " (no FSM event)\n";
        return true;
    }

    // state machine
    // TBD add handling of connection timeout event (KEEP-ALIVE), with transition to CLOSED
    // Note: empty "default:" lines are for gcc's benefit which would otherwise spit warnings
    int oldState = fsm.getState();

    switch (fsm.getState()) {
        case RAQSAC_S_INIT:
            switch (event) {
                case RAQSAC_E_OPEN_PASSIVE:
                    FSM_Goto(fsm, RAQSAC_S_LISTEN);
                    break;

                case RAQSAC_E_OPEN_ACTIVE:
                    FSM_Goto(fsm, RAQSAC_S_ESTABLISHED);
                    break;

                default:
                    break;
            }
            break;

        case RAQSAC_S_LISTEN:
            switch (event) {
                case RAQSAC_E_OPEN_ACTIVE:
                    FSM_Goto(fsm, RAQSAC_S_SYN_SENT);
                    break;

                case RAQSAC_E_RCV_SYN:
                    FSM_Goto(fsm, RAQSAC_S_SYN_RCVD);
                    break;

                default:
                    break;
            }
            break;

        case RAQSAC_S_SYN_RCVD:
            switch (event) {

                default:
                    break;
            }
            break;

        case RAQSAC_S_SYN_SENT:
            switch (event) {

                case RAQSAC_E_RCV_SYN:
                    FSM_Goto(fsm, RAQSAC_S_SYN_RCVD);
                    break;

                default:
                    break;
            }
            break;

        case RAQSAC_S_ESTABLISHED:
            switch (event) {

                default:
                    break;
            }
            break;

        case RAQSAC_S_CLOSED:
            break;
    }

    if (oldState != fsm.getState()) {
        EV_INFO << "Transition: " << stateName(oldState) << " --> " << stateName(fsm.getState()) << "  (event was: " << eventName(event) << ")\n";
        EV_DEBUG_C("testing") << raqsacMain->getName() << ": " << stateName(oldState) << " --> " << stateName(fsm.getState()) << "  (on " << eventName(event) << ")\n";

        // cancel timers, etc.
        stateEntered(fsm.getState(), oldState, event);
    }
    else {
        EV_DETAIL << "Staying in state: " << stateName(fsm.getState()) << " (event was: " << eventName(event) << ")\n";
    }

    return fsm.getState() != RAQSAC_S_CLOSED;
}

void RaqsacConnection::stateEntered(int state, int oldState, RaqsacEventCode event)
{
    // cancel timers
    switch (state) {
        case RAQSAC_S_INIT:
            // we'll never get back to INIT
            break;

        case RAQSAC_S_LISTEN:
            // we may get back to LISTEN from SYN_RCVD
            break;

        case RAQSAC_S_SYN_RCVD:
        case RAQSAC_S_SYN_SENT:
            break;

        case RAQSAC_S_ESTABLISHED:
            // we're in ESTABLISHED, these timers are no longer needed
            // NDP_I_ESTAB notification moved inside event processing
            break;

        case RAQSAC_S_CLOSED:
            sendIndicationToApp(RAQSAC_I_CLOSED);
            // all timers need to be cancelled
            raqsacAlgorithm->connectionClosed();
            break;
    }
}
} // namespace NDP
} // namespace inet

