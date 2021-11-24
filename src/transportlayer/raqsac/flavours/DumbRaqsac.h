
#ifndef __INET_DumbRaqsac_H
#define __INET_DumbRaqsac_H

#include <inet/common/INETDefs.h>
#include "../../raqsac/RaqsacAlgorithm.h"

namespace inet {

namespace raqsac {

/**
 * State variables for DumbRaqsac.
 */
class INET_API DumbRaqsacStateVariables : public RaqsacStateVariables
{
  public:
    //...
};

/**
 * A very-very basic NdpAlgorithm implementation, with hardcoded
 * retransmission timeout and no other sophistication. It can be
 * used to demonstrate what happened if there was no adaptive
 * timeout calculation, delayed acks, silly window avoidance,
 * congestion control, etc.
 */
class INET_API DumbRaqsac : public RaqsacAlgorithm
{
  protected:
    DumbRaqsacStateVariables *& state;    // alias to TCLAlgorithm's 'state'

    cMessage *rexmitTimer;    // retransmission timer

  protected:
    /** Creates and returns a DumbRaqsacStateVariables object. */
    virtual RaqsacStateVariables *createStateVariables() override
    {
        return new DumbRaqsacStateVariables();
    }

  public:
    /** Ctor */
    DumbRaqsac();

    virtual ~DumbRaqsac();

    virtual void initialize() override;

    virtual void connectionClosed() override;

    virtual void processTimer(cMessage *timer, RaqsacEventCode& event) override;

    virtual void dataSent(uint32 fromseq) override;
};

} // namespace NDP

} // namespace inet

#endif // ifndef __INET_DumbRaqsac_H

