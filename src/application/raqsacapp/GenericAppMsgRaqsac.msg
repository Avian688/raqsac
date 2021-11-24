

import inet.common.INETDefs;
import inet.common.Units;
import inet.common.packet.chunk.Chunk;

namespace inet;

class GenericAppMsgRaqsac extends FieldsChunk
{
    int numSymbolsToSend;  //  if this is a reader(receiver)
    bool serverClose;        // if true, server should close the
    uint32_t sequenceNumber; //new TODO
 }



