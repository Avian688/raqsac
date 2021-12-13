package raqsac.transportlayer.raqsac;

import raqsac.transportlayer.contract.IRaqsac;

simple Raqsac like IRaqsac
{
    parameters:
        int initialWindow = default(2);  // initialWindow size NDP-style (sender pushes IW pacakets)
        int ssthresh = default(10);
        string raqsacAlgorithmClass = default("DumbRaqsac");
        @display("i=block/wheelbarrow");

        @signal[packetReceivedFromUpper](type=cPacket);
        @signal[packetReceivedFromLower](type=cPacket);
    gates:
        input appIn @labels(RaqsacCommand/down) @messageKinds(inet::RaqsacCommandCode);
        input ipIn @labels(RaqsacHeader,Ipv4ControlInfo/up,Ipv6ControlInfo/up);
        output appOut @labels(RaqsacCommand/up) @messageKinds(inet::RaqsacStatusInd);
        output ipOut @labels(RaqsacHeader,Ipv4ControlInfo/down,Ipv6ControlInfo/down);
}

