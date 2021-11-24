

package raqsac.transportlayer.contract;

moduleinterface IRaqsac
{
    @display("i=block/wheelbarrow");
    gates:
        input appIn @labels(RaqsacCommand/down);
        input ipIn @labels(RaqsacHeader,IPv4ControlInfo/up,IPv6ControlInfo/up);
        output appOut @labels(RaqsacCommand/up);
        output ipOut @labels(RaqsacHeader,IPv4ControlInfo/down,IPv6ControlInfo/down);
}
