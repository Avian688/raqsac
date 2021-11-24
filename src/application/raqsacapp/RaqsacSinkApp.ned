
package raqsac.application.raqsacapp;

import inet.applications.contract.IApp;

simple RaqsacSinkApp like IApp
{
    parameters:
        volatile string localAddress = default(""); // may be left empty ("")
        volatile int localPort; // port number to listen on
        int opcode;
        bool recordStatistics = default(true);
        
        @class("inet::RaqsacSinkApp");
        @display("i=block/sink");

        @signal[numRcvTrimmedHeaderSig](type=double);
        @statistic[numRcvTrimmedHeaderSig](title="numRcvTrimmedHeader"; source=numRcvTrimmedHeaderSig; unit=s; record=last);

        @signal[fctRecordv3](type=double);
        @statistic[fctRecordv3](title="Scdp Session's flow completion time"; source=fctRecordv3; unit=s; record=last);

        @signal[packetReceived](type=inet::Packet);
        @statistic[packetReceived](title="packets (symbols) received"; source=packetReceived; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);

        @statistic[instThroughput](title="instThroughput"; source="throughputA(packetReceived)"; unit=bps; record=mean,vector); //ResultsFilter.cc
        @signal[goodputSig](type=double);
        @statistic[mohThroughput](title="mohThroughput"; source=goodputSig; unit=bps; record=last); //ResultsFilter.cc
    gates:
        input socketIn @labels(RaqsacCommand/up);
        output socketOut @labels(RaqsacCommand/down);
}

