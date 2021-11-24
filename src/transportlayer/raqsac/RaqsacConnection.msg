

import inet.common.INETDefs;
import inet.common.packet.Packet;
import inet.networklayer.common.L3Address;

cplusplus {{
#include "Raqsac.h"
#include "RaqsacAlgorithm.h"
#include "RaqsacConnection.h"
#include "raqsac_common/RaqsacHeader.h"
}}

namespace inet::raqsac;

class RaqsacAlgorithm extends cObject { @existingClass; }

struct RaqsacStateVariables
{
    @existingClass;
    @descriptor(readonly);
    
    bool active;
    
    unsigned int request_id;
    unsigned int internal_request_id;
    uint32 IW;
    bool connFinished;
    uint32 numPacketsToGet;
    uint32 numSymbolsToSend;

    unsigned int numRcvdPkt;
    unsigned int numRcvTrimmedHeader;
    unsigned int numTimesDecodingFailed;


    uint32 numberReceivedPackets;
    uint32 numberSentPackets;

    bool connNotAddedYet;
    bool isfinalReceivedPrintedOut;
};

class RaqsacConnection extends cObject
{
    @existingClass;
    @descriptor(readonly);

    int socketId;    // identifies connection within the app
    int listeningSocketId;    // identifies listening connection within the app

    // socket pair
    L3Address localAddr;
    L3Address remoteAddr;
    int localPort;
    int remotePort;

    RaqsacStateVariables *state;

    RaqsacAlgorithm *RaqsacAlgorithm;
    int fsmState;
};

