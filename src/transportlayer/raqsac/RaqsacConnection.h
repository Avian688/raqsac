#ifndef __INET_RaqsacConnection_H
#define __INET_RaqsacConnection_H

#include <inet/common/INETDefs.h>
#include <inet/networklayer/common/L3Address.h>
#include <inet/common/packet/ChunkQueue.h>
#include <queue>

#include "../../transportlayer/raqsac/Raqsac.h"
#include "../raqsac/raqsac_common/RaqsacHeader.h"

namespace inet {

class RaqsacCommand;
class RaqsacOpenCommand;

namespace raqsac {

class RaqsacHeader;
class RaqsacAlgorithm;

enum RaqsacState
{
    RAQSAC_S_INIT = 0, RAQSAC_S_CLOSED = FSM_Steady(1), RAQSAC_S_LISTEN = FSM_Steady(2), RAQSAC_S_SYN_SENT = FSM_Steady(3), RAQSAC_S_SYN_RCVD = FSM_Steady(4), RAQSAC_S_ESTABLISHED = FSM_Steady(5),
};

//
// Event, strictly for the FSM state transition purposes.
// DO NOT USE outside performStateTransition()!
//
enum RaqsacEventCode
{
    RAQSAC_E_IGNORE,

    // app commands
    RAQSAC_E_OPEN_ACTIVE,
    RAQSAC_E_OPEN_PASSIVE,
    RAQSAC_E_RCV_DATA,
    RAQSAC_E_RCV_SYN,
};

/**
 * Contains state variables ("TCB") for NDP.
 *
 * RaqsacStateVariables is effectively a "struct" -- it only contains
 * public data members. (Only declared as a class so that we can use
 * cObject as base class and make it possible to inspect
 * it in Tkenv.)
 *
 * RaqsacStateVariables only contains variables needed to implement
 * the "base" (RFC 793) NDP. More advanced NDP variants are encapsulated
 * into RaqsacAlgorithm subclasses which can have their own state blocks,
 * subclassed from RaqsacStateVariables. See RaqsacAlgorithm::createStateVariables().
 */
class INET_API RaqsacStateVariables : public cObject
{
public:
    RaqsacStateVariables();
    virtual std::string str() const override;
    virtual std::string detailedInfo() const OMNETPP5_CODE(override);

public:
    bool active;    // set if the connection was initiated by an active open
    unsigned int request_id;
    unsigned int internal_request_id;

    int IW;  //initial window size
    int cwnd;
    int ssthresh;
    int receivedPacketsInWindow;
    int sentPullsInWindow;
    bool connFinished;
    int numPacketsToGet;
    int numSymbolsToSend;
    uint32 esi;
    uint8_t sbn;
    unsigned int priorityValue;
    unsigned int numRcvdPkt;
    unsigned int numRcvTrimmedHeader;
    unsigned int numTimesDecodingFailed;
    int numberReceivedPackets;
    int numberSentPackets;

    bool connNotAddedYet;
    bool isfinalReceivedPrintedOut;
    bool redoDecoding;

    bool sendPulls;
};

class INET_API RaqsacConnection : public cSimpleModule
{
public:
    static simsignal_t cwndSignal;

    struct EncodingSymbols
    {
        unsigned int ESI;
        Packet *msg;
    };
    typedef std::list<EncodingSymbols> EncodingSymbolsList;
    EncodingSymbolsList encodingSymbolsList;
    EncodingSymbolsList receivedSymbolsList;
    // connection identification by apps: socketId
    int socketId = -1;    // identifies connection within the app
    int getSocketId() const
    {
        return socketId;
    }
    void setSocketId(int newSocketId)
    {
        ASSERT(socketId == -1);
        socketId = newSocketId;
    }

    int listeningSocketId = -1; // identifies listening connection within the app
    int getListeningSocketId() const
    {
        return listeningSocketId;
    }

    // socket pair
    L3Address localAddr;
    const L3Address& getLocalAddr() const
    {
        return localAddr;
    }
    L3Address remoteAddr;
    const L3Address& getRemoteAddr() const
    {
        return remoteAddr;
    }
    int localPort = -1;
    int remotePort = -1;
protected:
    Raqsac *raqsacMain = nullptr;    // RaQSac module

    // NDP state machine
    cFSM fsm;

    // variables associated with RaQSac state
    RaqsacStateVariables *state = nullptr;
public:
    virtual int getNumRcvdPackets();
    virtual bool isConnFinished();
    virtual void setConnFinished();

protected:
    //cQueue pullQueue;
    cPacketQueue pullQueue;
    // RaQSac behavior in data transfer state
    RaqsacAlgorithm *raqsacAlgorithm = nullptr;
    RaqsacAlgorithm* getRaqsacAlgorithm() const
    {
        return raqsacAlgorithm;
    }

protected:
    /** @name FSM transitions: analysing events and executing state transitions */
    //@{
    /** Maps app command codes (msg kind of app command msgs) to RAQSAC_E_xxx event codes */
    virtual RaqsacEventCode preanalyseAppCommandEvent(int commandCode);
    /** Implemements the pure NDP state machine */
    virtual bool performStateTransition(const RaqsacEventCode &event);
    /** Perform cleanup necessary when entering a new state, e.g. cancelling timers */
    virtual void stateEntered(int state, int oldState, RaqsacEventCode event);
    //@}

    /** @name Processing app commands. Invoked from processAppCommand(). */
    //@{
    virtual void process_OPEN_ACTIVE(RaqsacEventCode &event, RaqsacCommand *raqsacCommand, cMessage *msg);
    virtual void process_OPEN_PASSIVE(RaqsacEventCode &event, RaqsacCommand *raqsacCommand, cMessage *msg);

    /**
     * Process incoming NDP segment. Returns a specific event code (e.g. RAQSAC_E_RCV_SYN)
     * which will drive the state machine.
     */
    virtual RaqsacEventCode process_RCV_SEGMENT(Packet *packet, const Ptr<const RaqsacHeader> &ndpseg, L3Address src, L3Address dest);
    virtual RaqsacEventCode processSegmentInListen(Packet *packet, const Ptr<const RaqsacHeader> &ndpseg, L3Address src, L3Address dest);

    virtual RaqsacEventCode processSegment1stThru8th(Packet *packet, const Ptr<const RaqsacHeader> &ndpseg);

    //@}
    /** Utility: clone a listening connection. Used for forking. */
    //virtual RaqsacConnection *cloneListeningConnection();
    //virtual void initClonedConnection(RaqsacConnection *listenerConn);
    /** Utility: creates send/receive queues and RaqsacAlgorithm */
    virtual void initConnection(RaqsacOpenCommand *openCmd);

    /** Utility: set snd_mss, rcv_wnd and sack in newly created state variables block */
    virtual void configureStateVariables();

    /** Utility: returns true if the connection is not yet accepted by the application */
    virtual bool isToBeAccepted() const
    {
        return listeningSocketId != -1;
    }
public:
    void pushSymbol();

    virtual void sendInitialWindow();

    /** Utility: adds control info to segment and sends it to IP */
    virtual void sendToIP(Packet *packet, const Ptr<RaqsacHeader> &raqsacseg);

    virtual bool isDecoderSuccessful();
    virtual void addRequestToPullsQueue();
    virtual void sendRequestFromPullsQueue();

    virtual int getPullsQueueLength();

    /** Utility: start a timer */
    void scheduleTimeout(cMessage *msg, simtime_t timeout)
    {
        raqsacMain->scheduleAt(simTime() + timeout, msg);
    }

protected:
    /** Utility: cancel a timer */
    cMessage* cancelEvent(cMessage *msg)
    {
        return raqsacMain->cancelEvent(msg);
    }

    /** Utility: send IP packet */
    virtual void sendToIP(Packet *pkt, const Ptr<RaqsacHeader> &raqsacseg, L3Address src, L3Address dest);

    /** Utility: sends packet to application */
    virtual void sendToApp(cMessage *msg);

    /** Utility: sends status indication (RAQSAC_I_xxx) to application */
    virtual void sendIndicationToApp(int code, const int id = 0);

    /** Utility: sends RAQSAC_I_ESTABLISHED indication with NDPConnectInfo to application */
    virtual void sendEstabIndicationToApp();

public:
    /** Utility: prints local/remote addr/port and app gate index/connId */
    virtual void printConnBrief() const;
    /** Utility: prints important header fields */
    static void printSegmentBrief(Packet *packet, const Ptr<const RaqsacHeader> &raqsacseg);
    /** Utility: returns name of RAQSAC_S_xxx constants */
    static const char* stateName(int state);
    /** Utility: returns name of RAQSAC_E_xxx constants */
    static const char* eventName(int event);
    /** Utility: returns name of RAQSAC_I_xxx constants */
    static const char* indicationName(int code);

public:
    RaqsacConnection()
    {
    }
    RaqsacConnection(const RaqsacConnection &other)
    {
    }
    void initialize()
    {
    }

    /**
     * The "normal" constructor.
     */
    void initConnection(Raqsac *mod, int socketId);

    /**
     * Destructor.
     */
    virtual ~RaqsacConnection();

    int getLocalPort() const
    {
        return localPort;
    }
    L3Address getLocalAddress() const
    {
        return localAddr;
    }

    int getRemotePort() const
    {
        return remotePort;
    }
    L3Address getRemoteAddress() const
    {
        return remoteAddr;
    }

    virtual void segmentArrivalWhileClosed(Packet *packet, const Ptr<const RaqsacHeader> &raqsacseg, L3Address src, L3Address dest);

    /** @name Various getters **/
    //@{
    int getFsmState() const
    {
        return fsm.getState();
    }
    RaqsacStateVariables* getState()
    {
        return state;
    }
    RaqsacAlgorithm* getRaqsacAlgorithm()
    {
        return raqsacAlgorithm;
    }
    Raqsac* getRaqsacMain()
    {
        return raqsacMain;
    }

    virtual bool processTimer(cMessage *msg);

    virtual bool processRaqsacSegment(Packet *packet, const Ptr<const RaqsacHeader> &raqsacseg, L3Address srcAddr, L3Address destAddr);

    virtual bool processAppCommand(cMessage *msg);

    virtual void handleMessage(cMessage *msg);

    /**
     * Utility: converts a given simtime to a timestamp (TS).
     */
    static uint32 convertSimtimeToTS(simtime_t simtime);

    /**
     * Utility: converts a given timestamp (TS) to a simtime.
     */
    static simtime_t convertTSToSimtime(uint32 timestamp);

};

} // namespace RaQSac

} // namespace inet

#endif // ifndef __INET_RaqsacConnection_H

