#ifndef __INET_NdpBasicClientApp_H
#define __INET_NdpBasicClientApp_H

#include <inet/common/INETDefs.h>
#include <inet/common/lifecycle/NodeStatus.h>
#include <inet/common/lifecycle/ILifecycle.h>
#include "NdpAppBase.h"

namespace inet {

/**
 * An example request-reply based client application.
 */
class INET_API NdpBasicClientApp : public NdpAppBase
{
protected:
    cMessage *timeoutMsg = nullptr;

    simtime_t startTime;
    simtime_t stopTime;
    NodeStatus *nodeStatus = nullptr;

    virtual void rescheduleOrDeleteTimer(simtime_t d, short int msgKind);

    virtual int numInitStages() const override
    {
        return NUM_INIT_STAGES;
    }

    // Initializes the client application, calls the initialize method of NdpAppBase
    virtual void initialize(int stage) override;

    // Handles message sent to the application. Most importantly the method handles
    // a self timer sent on startup to connect to the socket.
    virtual void handleTimer(cMessage *msg) override;

    virtual void socketEstablished(NdpSocket *socket) override;

    // No data should arrive in the NDP client application as this is handled by the
    // NdpConnection. Method must override the ApplicationBase method.
    virtual void socketDataArrived(NdpSocket *socket, Packet *msg, bool urgent) override
    {
        return;
    }
    ;
    virtual void socketClosed(NdpSocket *socket) override;
    virtual void socketFailure(NdpSocket *socket, int code) override;

    virtual void handleStartOperation(LifecycleOperation *operation) override;
    virtual void handleStopOperation(LifecycleOperation *operation) override;

    // TODO - not implemented
    virtual void handleCrashOperation(LifecycleOperation *operation) override;

    virtual bool isNodeUp();

    virtual void close() override;

public:
    NdpBasicClientApp()
    {
    }
    virtual ~NdpBasicClientApp();
};

} // namespace inet

#endif // ifndef __INET_NdpBasicClientApp_H

