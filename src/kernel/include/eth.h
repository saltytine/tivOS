#include "util.h"

#ifndef NET_ETH_H
#define NET_ETH_H

#define ETHERTYPE_PUP 0x0200
#define ETHERTYPE_SPRITE 0x0500
#define ETHERTYPE_IP 0x0800
#define ETHERTYPE_ARP 0x0806
#define ETHERTYPE_REVARP 0x8035
#define ETHERTYPE_AT 0x809B
#define ETHERTYPE_AARP 0x80F3
#define ETHERTYPE_VLAN 0x8100
#define ETHERTYPE_IPX 0x8137
#define ETHERTYPE_IPV6 0x86dd
#define ETHERTYPE_LOOPBACK 0x9000

#define MAC_BYTE_SIZE 6  // MAC addresses are always 6 bytes long
#define IPv4_BYTE_SIZE 4 // IPv4 addresses are always 4 bytes long

typedef struct ethHeader {
  uint8_t  dest[MAC_BYTE_SIZE];
  uint8_t  src[MAC_BYTE_SIZE];
  uint16_t type; // ethertype
} __attribute__((packed)) ethHeader;

void netEthHandle(void *_nic, void *packet, uint32_t size);
void netEthSend(void *_nic, void *packet, uint32_t size, uint8_t *targetMac,
                uint16_t ethertype);

uint8_t macBroadcast[MAC_BYTE_SIZE];
uint8_t macZero[MAC_BYTE_SIZE];
uint8_t addressNull[IPv4_BYTE_SIZE];
uint8_t addressBroadcast[IPv4_BYTE_SIZE];

void dbgMac(uint8_t *mac);
void dbgIp(uint8_t *ip);

#endif
