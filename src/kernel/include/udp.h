#include "eth.h"
#include "ipv4.h"
#include "linked_list.h"
#include "netports.h"
#include "util.h"

#ifndef NET_UDP_H
#define NET_UDP_H

typedef struct udpHeader {
  uint16_t srcPort;
  uint16_t destPort;
  uint16_t length;
  uint16_t checksum;
} __attribute__((packed)) udpHeader;

#define NET_UDP_CARRY(packet)                                                  \
  (sizeof(ethHeader) + NET_IPv4(packet)->ihl * sizeof(uint32_t))
#define NET_UDP_CARRY_BARE (sizeof(ethHeader) + sizeof(IPv4header))
#define NET_UDP(packet)                                                        \
  ((udpHeader *)((size_t)(packet) + NET_UDP_CARRY(packet)))

#define UDP_MAX_TOTAL_DATA 524288

typedef struct udpBuffer {
  LLheader _ll;

  uint8_t ip[IPv4_BYTE_SIZE];
  uint8_t remotePort;

  void  *buff;
  size_t len;
} udpBuffer;

typedef struct UdpConnection {
  LLheader _ll;

  uint8_t  ip[IPv4_BYTE_SIZE];
  uint16_t localPort;
  uint16_t remotePort;

  size_t    totalData;
  LLcontrol dsBuffers; // struct udpBuffer
} UdpConnection;

typedef struct UdpStore {
  LLcontrol dsUdpConnections; // struct UdpConnection
  Spinlock  LOCK_LL_UDP_CONNS;

  NetPorts netPortsUdp;
} UdpStore;

typedef struct UdpSocket {
  int timesOpened;

  Spinlock LOCK_SOCKET;

  // here as it can't be closed, duplicated or anything. only we own it
  UdpConnection *conn;
  bool           connected; // has connect() been called
} UdpSocket;

typedef struct sockaddr_in_linux {
  uint16_t sin_family;
  uint16_t sin_port;
  uint8_t  sin_addr[4];
  uint8_t  sin_zero[8];
} sockaddr_in_linux;

void netUdpStoreInit(UdpStore *store);
void netUdpHandle(void *_nic, void *packet, uint32_t size);

void netSocketUdpInit(void *_fd);

#endif
