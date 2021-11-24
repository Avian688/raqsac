# NDP - Re-architecting datacenter networks and stacks for low latency and high performance OMNeT++/INET Implementation



**Changes to INET **



**inet/networklayer/ipv4**



**Replace following method in Ipv4NatTable.cc**



``` INetfilter::IHook::Result Ipv4NatTable::processPacket(Packet *packet, INetfilter::IHook::Type type)
{
    Enter_Method_Silent();
    auto lt = natEntries.lower_bound(type);
    auto ut = natEntries.upper_bound(type);
    for (; lt != ut; lt++) {
        const auto& packetFilter = lt->second.first;
        const auto& natEntry = lt->second.second;
        // TODO: this might be slow for too many filters
        if (packetFilter->matches(packet)) {
            auto& ipv4Header = removeNetworkProtocolHeader<Ipv4Header>(packet);
            if (!natEntry.getDestAddress().isUnspecified())
                ipv4Header->setDestAddress(natEntry.getDestAddress());
            if (!natEntry.getSrcAddress().isUnspecified())
                ipv4Header->setSrcAddress(natEntry.getSrcAddress());
            auto transportProtocol = ipv4Header->getProtocol();
#ifdef WITH_UDP
            if (transportProtocol == &Protocol::udp) {
                auto& udpHeader = removeTransportProtocolHeader<UdpHeader>(packet);
                // TODO: if (!Udp::verifyCrc(Protocol::ipv4, udpHeader, packet))
                auto udpData = packet->peekData();
                if (natEntry.getDestPort() != -1)
                    udpHeader->setDestPort(natEntry.getDestPort());
                if (natEntry.getSrcPort() != -1)
                    udpHeader->setSrcPort(natEntry.getSrcPort());
                Udp::insertCrc(&Protocol::ipv4, ipv4Header->getSrcAddress(), ipv4Header->getDestAddress(), udpHeader, packet);
                insertTransportProtocolHeader(packet, Protocol::udp, udpHeader);
            }
            else
#endif
#ifdef WITH_TCP_COMMON
            if (transportProtocol == &Protocol::tcp) {
                auto& tcpHeader = removeTransportProtocolHeader<tcp::TcpHeader>(packet);
                // TODO: if (!Tcp::verifyCrc(Protocol::ipv4, tcpHeader, packet))
                auto tcpData = packet->peekData();
                if (natEntry.getDestPort() != -1)
                    tcpHeader->setDestPort(natEntry.getDestPort());
                if (natEntry.getSrcPort() != -1)
                    tcpHeader->setSrcPort(natEntry.getSrcPort());
                tcp::TcpCrcInsertion::insertCrc(&Protocol::ipv4, ipv4Header->getSrcAddress(), ipv4Header->getDestAddress(), tcpHeader, packet);
                insertTransportProtocolHeader(packet, Protocol::tcp, tcpHeader);
            }
            else
#endif
            if (transportProtocol == &Protocol::ndp) {
                auto& ndpHeader = removeTransportProtocolHeader<ndp::NdpHeader>(packet);
                auto ndpData = packet->peekData();
                if (natEntry.getDestPort() != -1)
                    ndpHeader->setDestPort(natEntry.getDestPort());
                if (natEntry.getSrcPort() != -1)
                    ndpHeader->setSrcPort(natEntry.getSrcPort());
                insertTransportProtocolHeader(packet, Protocol::ndp, ndpHeader);
            }
            else
                throw cRuntimeError("Unknown protocol: '%s'", transportProtocol ? transportProtocol->getName() : std::to_string((int)ipv4Header->getProtocolId()).c_str());
            insertNetworkProtocolHeader(packet, Protocol::ipv4, ipv4Header);
            break;
        }
    }
    return ACCEPT;
} 
```

**inet/common/**



**Add line in Protocol.cc:**
```
const Protocol Protocol::ndp("ndp", "NDP", Protocol::TransportLayer);
```
**Add line in Protocol.h**
```
static const Protocol ndp;
```
**Replace the following code in ProtocolGroup.cc**
```
ProtocolGroup ProtocolGroup::ipprotocol("ipprotocol", {
{ 1, &Protocol::icmpv4 },
{ 2, &Protocol::igmp },
{ 4, &Protocol::ipv4 },
{ 6, &Protocol::tcp },
{ 144, &Protocol::ndp }, //unassigned number for NDP
{ 8, &Protocol::egp },
{ 9, &Protocol::igp },
{ 17, &Protocol::udp },
{ 36, &Protocol::xtp },
{ 41, &Protocol::ipv6 },
{ 46, &Protocol::rsvpTe },
{ 48, &Protocol::dsr },
{ 58, &Protocol::icmpv6 },
{ 89, &Protocol::ospf },
{ 103, &Protocol::pim },
{ 132, &Protocol::sctp },
{ 135, &Protocol::mobileipv6 },
{ 138, &Protocol::manet },



{ 249, &Protocol::linkStateRouting }, // INET specific: Link State Routing Protocol
{ 250, &Protocol::flooding }, // INET specific: Probabilistic Network Protocol
{ 251, &Protocol::probabilistic }, // INET specific: Probabilistic Network Protocol
{ 252, &Protocol::wiseRoute }, // INET specific: Probabilistic Network Protocol
{ 253, &Protocol::nextHopForwarding }, // INET specific: Next Hop Forwarding
{ 254, &Protocol::echo }, // INET specific: Echo Protocol
});
```
