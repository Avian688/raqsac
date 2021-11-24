#ifndef __Raqsac_RaqsacAppBase_H
#define __Raqsac_RaqsacAppBase_H

#include <inet/common/INETDefs.h>
#include <inet/applications/base/ApplicationBase.h>
#include "../../transportlayer/contract/raqsac/RaqsacSocket.h"

namespace inet {

/**
 * Base class for clients app for RaQSac-based request-reply protocols or apps.
 * Handles a single session (and RaQSac connection) at a time.
 *
 * It needs the following NED parameters: localAddress, localPort, connectAddress, connectPort.
 */
class INET_API RaqsacAppBase : public ApplicationBase, public RaqsacSocket::ICallback
{
protected:
    RaqsacSocket socket;

protected:
    // Initializes the application, binds the socket to the local address and port.
    virtual void initialize(int stage) override;

    virtual int numInitStages() const override
    {
        return NUM_INIT_STAGES;
    }

    virtual void handleMessageWhenUp(cMessage *msg) override;
    virtual void finish() override;

    /* Utility functions */
    // Creates a socket connection based on the NED parameters specified.
    virtual void connect();
    // Closes the socket.
    virtual void close();

    /* RaqsacSocket::ICallback callback methods */
    virtual void handleTimer(cMessage *msg) = 0;

    // Called once the socket is established. Currently does nothing but set the status string.
    virtual void socketEstablished(RaqsacSocket *socket) override;

    //Called once a packet arrives at the application.
    virtual void socketDataArrived(RaqsacSocket *socket, Packet *msg, bool urgent) override;

    virtual void socketAvailable(RaqsacSocket *socket, RaqsacAvailableInfo *availableInfo) override
    {
        socket->accept(availableInfo->getNewSocketId());
    }
    virtual void socketPeerClosed(RaqsacSocket *socket) override;
    virtual void socketClosed(RaqsacSocket *socket) override;
    virtual void socketFailure(RaqsacSocket *socket, int code) override;
    virtual void socketStatusArrived(RaqsacSocket *socket, RaqsacStatusInfo *status)
    override
    {
        delete status;
    }
    virtual void socketDeleted(RaqsacSocket *socket) override
    {
    }
    virtual int getPriorityValue(int flowSize);
};

} // namespace inet

#endif // ifndef __INET_RAPTORaqsacAppBase_H

