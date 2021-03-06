#include <string.h>
#include <algorithm>
#include <inet/networklayer/contract/IL3AddressType.h>
#include <inet/networklayer/common/IpProtocolId_m.h>
#include <inet/applications/common/SocketTag_m.h>
#include <inet/common/INETUtils.h>
#include <inet/common/packet/Message.h>
#include <inet/networklayer/common/EcnTag_m.h>
#include <inet/networklayer/common/IpProtocolId_m.h>
#include <inet/networklayer/common/L3AddressResolver.h>
#include <inet/networklayer/common/L3AddressTag_m.h>
#include <inet/networklayer/common/HopLimitTag_m.h>
#include <inet/common/Protocol.h>
#include <inet/common/TimeTag_m.h>

#include "NdpAlgorithm.h"
#include "NdpConnection.h"
#include "NdpSendQueue.h"
#include "Ndp.h"
#include "ndp_common/NdpHeader.h"
#include "../../application/ndpapp/GenericAppMsgNdp_m.h"
#include "../common/L4ToolsNdp.h"
#include "../contract/ndp/NdpCommand_m.h"
namespace inet {

namespace ndp {

//
// helper functions
//

const char* NdpConnection::stateName(int state)
{
#define CASE(x)    case x: \
        s = #x + 5; break
    const char *s = "unknown";
    switch (state) {
        CASE(NDP_S_INIT)
;            CASE(NDP_S_CLOSED);
            CASE(NDP_S_LISTEN);
            CASE(NDP_S_ESTABLISHED);
        }
    return s;
#undef CASE
}

const char* NdpConnection::eventName(int event)
{
#define CASE(x)    case x: \
        s = #x + 5; break
    const char *s = "unknown";
    switch (event) {
        CASE(NDP_E_IGNORE)
;            CASE(NDP_E_OPEN_ACTIVE);
            CASE(NDP_E_OPEN_PASSIVE);
        }
    return s;
#undef CASE
}

const char* NdpConnection::indicationName(int code)
{
#define CASE(x)    case x: \
        s = #x + 5; break
    const char *s = "unknown";
    switch (code) {
        CASE(NDP_I_DATA);
        CASE(NDP_I_ESTABLISHED);
        CASE(NDP_I_PEER_CLOSED);

        }
    return s;
#undef CASE
}

void NdpConnection::sendToIP(Packet *packet, const Ptr<NdpHeader> &ndpseg)
{
    EV_TRACE << "NdpConnection::sendToIP" << endl;
    ndpseg->setSrcPort(localPort);
    ndpseg->setDestPort(remotePort);
    //EV_INFO << "Sending: " << endl;
    //printSegmentBrief(packet, ndpseg);
    IL3AddressType *addressType = remoteAddr.getAddressType();
    packet->addTagIfAbsent<DispatchProtocolReq>()->setProtocol(addressType->getNetworkProtocol());
    auto addresses = packet->addTagIfAbsent<L3AddressReq>();
    addresses->setSrcAddress(localAddr);
    addresses->setDestAddress(remoteAddr);
    insertTransportProtocolHeader(packet, Protocol::ndp, ndpseg);
    ndpMain->sendFromConn(packet, "ipOut");
}

void NdpConnection::sendToIP(Packet *packet, const Ptr<NdpHeader> &ndpseg, L3Address src, L3Address dest)
{
    //EV_INFO << "Sending: ";
    //printSegmentBrief(packet, ndpseg);
    IL3AddressType *addressType = dest.getAddressType();
    packet->addTagIfAbsent<DispatchProtocolReq>()->setProtocol(addressType->getNetworkProtocol());
    auto addresses = packet->addTagIfAbsent<L3AddressReq>();
    addresses->setSrcAddress(src);
    addresses->setDestAddress(dest);

    insertTransportProtocolHeader(packet, Protocol::ndp, ndpseg);
    ndpMain->sendFromConn(packet, "ipOut");
}

void NdpConnection::sendIndicationToApp(int code, const int id)
{
    EV_INFO << "Notifying app: " << indicationName(code) << endl;
    auto indication = new Indication(indicationName(code), code);
    NdpCommand *ind = new NdpCommand();
    ind->setNumRcvTrimmedHeader(state->numRcvTrimmedHeader);
    ind->setUserId(id);
    indication->addTag<SocketInd>()->setSocketId(socketId);
    indication->setControlInfo(ind);
    sendToApp(indication);
}

void NdpConnection::sendEstabIndicationToApp()
{
    EV_INFO << "Notifying app: " << indicationName(NDP_I_ESTABLISHED) << endl;
    auto indication = new Indication(indicationName(NDP_I_ESTABLISHED), NDP_I_ESTABLISHED);
    NdpConnectInfo *ind = new NdpConnectInfo();
    ind->setLocalAddr(localAddr);
    ind->setRemoteAddr(remoteAddr);
    ind->setLocalPort(localPort);
    ind->setRemotePort(remotePort);
    indication->addTag<SocketInd>()->setSocketId(socketId);
    indication->setControlInfo(ind);
    sendToApp(indication);
}

void NdpConnection::sendToApp(cMessage *msg)
{
    ndpMain->sendFromConn(msg, "appOut");
}

void NdpConnection::initConnection(NdpOpenCommand *openCmd)
{
    sendQueue = ndpMain->createSendQueue();
    sendQueue->setConnection(this);
    //create algorithm
    const char *ndpAlgorithmClass = openCmd->getNdpAlgorithmClass();

    if (!ndpAlgorithmClass || !ndpAlgorithmClass[0])
        ndpAlgorithmClass = ndpMain->par("ndpAlgorithmClass");

    ndpAlgorithm = check_and_cast<NdpAlgorithm*>(inet::utils::createOne(ndpAlgorithmClass));
    ndpAlgorithm->setConnection(this);
    // create state block
    state = ndpAlgorithm->getStateVariables();
    configureStateVariables();
    ndpAlgorithm->initialize();
}

void NdpConnection::configureStateVariables()
{
    state->IW = ndpMain->par("initialWindow");
    ndpMain->recordScalar("initialWindow=", state->IW);

}

// the receiver sends NACK when receiving a header
void NdpConnection::sendNackNdp(unsigned int nackNum)
{
    EV_INFO << "Sending Nack! NackNum: " << nackNum << endl;
    const auto &ndpseg = makeShared<NdpHeader>();
    ndpseg->setAckBit(false);
    ndpseg->setNackBit(true);
    ndpseg->setNackNo(nackNum);
    ndpseg->setSynBit(false);
    ndpseg->setIsDataPacket(false);
    ndpseg->setIsPullPacket(false);
    ndpseg->setIsHeader(false);
    std::string packetName = "NdpNack-" + std::to_string(nackNum);
    Packet *fp = new Packet(packetName.c_str());
    // send it
    sendToIP(fp, ndpseg);
}

void NdpConnection::sendAckNdp(unsigned int AckNum)
{
    EV_INFO << "Sending Ack! AckNum: " << AckNum << endl;
    const auto &ndpseg = makeShared<NdpHeader>();
    ndpseg->setAckBit(true);
    ndpseg->setAckNo(AckNum);
    ndpseg->setNackBit(false);
    ndpseg->setSynBit(false);
    ndpseg->setIsDataPacket(false);
    ndpseg->setIsPullPacket(false);
    ndpseg->setIsHeader(false);
    std::string packetName = "NdpAck-" + std::to_string(AckNum);
    Packet *fp = new Packet(packetName.c_str());
    // send it
    sendToIP(fp, ndpseg);
}

void NdpConnection::printConnBrief() const
{
    EV_DETAIL << "Connection " << localAddr << ":" << localPort << " to " << remoteAddr << ":" << remotePort << "  on socketId=" << socketId << "  in " << stateName(fsm.getState()) << endl;
}

void NdpConnection::printSegmentBrief(Packet *packet, const Ptr<const NdpHeader> &ndpseg)
{
    EV_STATICCONTEXT
    ;
    EV_INFO << "." << ndpseg->getSrcPort() << " > ";
    EV_INFO << "." << ndpseg->getDestPort() << ": ";

    if (ndpseg->getSynBit())
        EV_INFO << (ndpseg->getAckBit() ? "SYN+ACK " : "SYN ");
    if (ndpseg->getRstBit())
        EV_INFO << (ndpseg->getAckBit() ? "RST+ACK " : "RST ");
    if (ndpseg->getAckBit())
        EV_INFO << "ack " << ndpseg->getAckNo() << " ";
    EV_INFO << endl;
}

uint32 NdpConnection::convertSimtimeToTS(simtime_t simtime)
{
    ASSERT(SimTime::getScaleExp() <= -3);
    uint32 timestamp = (uint32) (simtime.inUnit(SIMTIME_MS));
    return timestamp;
}

simtime_t NdpConnection::convertTSToSimtime(uint32 timestamp)
{
    ASSERT(SimTime::getScaleExp() <= -3);
    simtime_t simtime(timestamp, SIMTIME_MS);
    return simtime;
}

} // namespace ndp

} // namespace inet

