//
// Copyright (C) 2004 Andras Varga
// Copyright (C) 2009-2010 Thomas Reschka
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#ifndef __INET_RaqsacAlgorithm_H
#define __INET_RaqsacAlgorithm_H

#include <inet/common/INETDefs.h>

#include "../../transportlayer/raqsac/raqsac_common/RaqsacHeader.h"
#include "RaqsacConnection.h"

namespace inet {

namespace raqsac {

/**
 * Abstract base class for NDP algorithms which encapsulate all behaviour
 * during data transfer state: flavour of congestion control, fast
 * retransmit/recovery, selective acknowledgement etc. Subclasses
 * may implement various sets and flavours of the above algorithms.
 */
class INET_API RaqsacAlgorithm : public cObject
{
protected:
    RaqsacConnection *conn;    // we belong to this connection
    RaqsacStateVariables *state;    // our state variables

    /**
     * Create state block (TCB) used by this NDP variant. It is expected
     * that every RaqsacAlgorithm subclass will have its own state block,
     * subclassed from RaqsacStateVariables. This factory method should
     * create and return a "blank" state block of the appropriate type.
     */
    virtual RaqsacStateVariables* createStateVariables() = 0;

public:
    /**
     * Ctor.
     */
    RaqsacAlgorithm()
    {
        state = nullptr;
        conn = nullptr;
    }

    /**
     * Virtual dtor.
     */
    virtual ~RaqsacAlgorithm()
    {
    }

    /**
     * Assign this object to a RaqsacAlgorithm. Its sendQueue and receiveQueue
     * must be set already at this time, because we cache their pointers here.
     */
    void setConnection(RaqsacConnection *_conn)
    {
        conn = _conn;
    }

    /**
     * Creates and returns the NDP state variables.
     */
    RaqsacStateVariables* getStateVariables()
    {
        if (!state)
            state = createStateVariables();

        return state;
    }

    /**
     * Should be redefined to initialize the object: create timers, etc.
     * This method is necessary because the RaqsacAlgorithm ptr is not
     * available in the constructor yet.
     */
    virtual void initialize()
    {
    }

    /**
     * Called when the connection closes, it should cancel all running timers.
     */
    virtual void connectionClosed() = 0;

    /**
     * Place to process timers specific to this RaqsacAlgorithm class.
     * RaqsacAlgorithm will invoke this method on any timer (self-message)
     * it doesn't recognize (that is, any timer other than the 2MSL,
     * CONN-ESTAB and FIN-WAIT-2 timers).
     *
     * Method may also change the event code (by default set to NDP_E_IGNORE)
     * to cause the state transition of NDP FSM.
     */
    virtual void processTimer(cMessage *timer, RaqsacEventCode &event) = 0;

    /**
     * Called after we sent data. This hook can be used to schedule the
     * retransmission timer, to start round-trip time measurement, etc.
     * The argument is the seqno of the first byte sent.
     */
    virtual void dataSent(uint32 fromseq) = 0;

};

} // namespace NDP

} // namespace inet

#endif // ifndef __INET_RaqsacAlgorithm_H

