import inet.common.INETDefs;
import inet.common.packet.chunk.Chunk;
import inet.transportlayer.contract.TransportHeaderBase;

namespace inet::raqsac;

cplusplus {{
const B RAQSAC_HEADER_LENGTH = B(20);  // default RaQSac header length: 20 bytes without options
}}

//
// Represents a RaQSac segment, to be used with the ~NDP module.
//
// RaQSac header fields not explicitly modelled: work on going
// - Data Offset (number of 32 bit words in the header): represented
//   by cMessage::length().
// - Reserved (reserved for future use)
// - Checksum (header checksum): modelled by cMessage::hasBitError()
// - Header Options: Currently only EOL, NOP, MSS, WS, SACK_PERMITTED, SACK and TS are implemented
// - Padding
//
// cMessage::getKind() may be set to an arbitrary value: RaQSac entities will
// ignore it and use only the header fields (synBit, rstBit).
//
class RaqsacHeader extends TransportHeaderBase
{
    chunkLength = RAQSAC_HEADER_LENGTH;

    // Source Port
    unsigned short srcPort; //2 Bytes
    // Destination Port
    unsigned short destPort; //2 Bytes
	bool isHeader;
	bool isPullPacket; //2 Bytes
	unsigned int priorityValue;
	unsigned int numSymbolsToSend;
	unsigned int pullSequenceNumber;
	unsigned int ESI; //2 Bytes
	unsigned int SBN; //2 Bytes
    bool rstBit; // RST: reset the connection
    bool synBit; // SYN: synchronize seq. numbers
}

cplusplus(RaqsacHeader) {{
  public:
    // implements TransportHeaderBase functions:
    virtual unsigned int getSourcePort() const override { return getSrcPort(); }
    virtual void setSourcePort(unsigned int port) override { setSrcPort(port); }
    virtual unsigned int getDestinationPort() const override { return getDestPort(); }
    virtual void setDestinationPort(unsigned int port) override { setDestPort(port); }
    virtual std::string str() const override;
}}

