#include <nic_controller.h>
#include <poll.h>
#include <udp.h>

// Layer4: UDP implementation

void netUdpStoreInit(UdpStore *store) {
  LinkedListInit(&store->dsUdpConnections, sizeof(UdpConnection));
  store->netPortsUdp.ports[0] = 0xff;
}

bool netUdpConnSearchCb(void *data, void *ctx) {
  UdpConnection *conn = data;
  UdpConnection *mockTarget = ctx;
  return conn->localPort == mockTarget->localPort &&
         (conn->remotePort == 0 || conn->remotePort == mockTarget->remotePort);
}

void netUdpHandle(void *_nic, void *packet, uint32_t size) {
  udpHeader *udp = NET_UDP(packet);

  NIC *nic = _nic;
  if (size < NET_UDP_CARRY(packet) + sizeof(udpHeader)) {
    debugf("[net::udp] Drop: Too small\n");
    return;
  }

  size_t dataOffset = NET_UDP_CARRY(packet) + sizeof(udpHeader);
  size_t payloadSize = size - dataOffset;

  if (switch_endian_16(udp->length) - sizeof(udpHeader) != payloadSize) {
    debugf("[net::udp] Drop: Invalid length field\n");
    return;
  }

  // used EXCLUSIVELY for lookups!
  UdpConnection mockConn = {.localPort = switch_endian_16(udp->destPort),
                            .remotePort = switch_endian_16(udp->srcPort)};
  IPv4header   *ipv4 = NET_IPv4(packet);

  // first make sure such an entry exists before saving the buffer
  UdpStore *udpStore = &nic->udp;
  spinlockAcquire(&udpStore->LOCK_LL_UDP_CONNS);
  UdpConnection *connAtmpt = LinkedListSearch(&udpStore->dsUdpConnections,
                                              netUdpConnSearchCb, &mockConn);
  bool           connExists = connAtmpt, connFull = false, connInvHost = false;
  if (connAtmpt && connAtmpt->totalData + payloadSize > UDP_MAX_TOTAL_DATA)
    connFull = true;
  if (connAtmpt && connAtmpt->ip[0] != 0 &&
      memcmp(connAtmpt->ip, ipv4->srcAddress, IPv4_BYTE_SIZE) != 0)
    connInvHost = true;
  spinlockRelease(&udpStore->LOCK_LL_UDP_CONNS);

  if (!connExists) {
    debugf("[net::udp] Drop: No open connection! local{%d} remote{%d}\n",
           mockConn.localPort, mockConn.remotePort);
    return;
  }

  if (connInvHost) {
    debugf("[net::udp] Drop: Sent from an invalid host\n");
    return;
  }

  if (connFull) {
    // todo: maybe discard older stuff? look it up!
    debugf("[net::udp] Drop: Cannot allocate more connection buffers\n");
    return;
  }

  uint8_t *original = (uint8_t *)packet;
  uint8_t *buff = malloc(payloadSize);
  memcpy(buff, &original[dataOffset], payloadSize);

  spinlockAcquire(&udpStore->LOCK_LL_UDP_CONNS);
  UdpConnection *conn = LinkedListSearch(&udpStore->dsUdpConnections,
                                         netUdpConnSearchCb, &mockConn);
  udpBuffer *object = LinkedListAllocate(&conn->dsBuffers, sizeof(udpBuffer));
  object->buff = buff;
  object->len = payloadSize;
  object->remotePort = mockConn.remotePort;
  memcpy(object->ip, ipv4->srcAddress, IPv4_BYTE_SIZE);
  conn->totalData += payloadSize;
  spinlockRelease(&udpStore->LOCK_LL_UDP_CONNS);

  pollInstanceRing((size_t)conn, EPOLLIN);
}
