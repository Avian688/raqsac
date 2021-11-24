

import inet.common.INETDefs;
import inet.networklayer.common.IpProtocolId;
import inet.networklayer.common.L3Address;

namespace inet;
//
// NDPR command codes, sent by the application to NDPR. These constants
// should be set as message kind on a message sent to the NDPR entity.
//
// @see ~NDPRCommand, ~NDPROpenCommand, ~IRaptoNDP
//
enum RaqsacCommandCode
{
    RAQSAC_C_OPEN_ACTIVE = 1;   // active open (must carry ~NDPOpenCommand)
    RAQSAC_C_OPEN_PASSIVE = 2;  // passive open (must carry ~NDPOpenCommand)
    RAQSAC_C_ACCEPT = 3;
}

//
// NDP indications, sent by NDP to the application. NDP will set these
// constants as message kind on messages it sends to the application.
//
// @see ~RaqsacCommand, ~NDPStatusInfo, ~INDP
//
enum RaqsacStatusInd
{
    RAQSAC_I_DATA = 1;              // data packet (set on data packet)
    RAQSAC_I_AVAILABLE = 3;         // conncetion available
    RAQSAC_I_ESTABLISHED = 4;       // connection established
    RAQSAC_I_PEER_CLOSED = 5;       // FIN received from remote NDP
    RAQSAC_I_CLOSED = 6;            // connection closed normally (via FIN exchange)
}

class RaqsacCommand extends cObject
{
   int userId = -1;	// id than can be freely used by the app
   unsigned int  numRcvTrimmedHeader = 0;   
   unsigned int  numTimesDecodingFailed = 0;   
}

class RaqsacOpenCommand extends RaqsacCommand
{
    L3Address localAddr; 
    L3Address remoteAddr;
    int localPort = -1;      
    int remotePort = -1;      
    string raqsacAlgorithmClass;
    unsigned int numSymbolsToSend;
    unsigned int priorityValue;
}

class RaqsacAvailableInfo extends RaqsacCommand
{
    L3Address localAddr;
    L3Address remoteAddr;
    int localPort;
    int remotePort;
    int newSocketId;
}

class RaqsacConnectInfo extends RaqsacCommand
{
    L3Address localAddr;
    L3Address remoteAddr;
    int localPort;
    int remotePort;
}

class RaqsacStatusInfo extends RaqsacCommand
{
    int state;
    string stateName;
    L3Address localAddr;
    L3Address remoteAddr;
    int localPort;
    int remotePort;
}
