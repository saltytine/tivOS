#include "eth.h"
#include "linked_list.h"
#include "util.h"

#ifndef NET_ARP_H
#define NET_ARP_H

typedef struct arpPacket {
  uint16_t hardwareType;
  uint16_t protocolType;
  uint8_t  hardwareSize;
  uint8_t  protocolSize;
  uint16_t opcode;
  uint8_t  senderMac[6];
  uint8_t  senderIp[4];
  uint8_t  targetMac[6];
  uint8_t  targetIp[4];
} __attribute__((packed)) arpPacket;

#define ARP_OP_REQUEST 0x01
#define ARP_OP_REPLY 0x02

#define ARP_HARDWARE_TYPE 0x0001
#define ARP_PROTOCOL_TYPE 0x0800

#define ARP_TIMEOUT 500

#define NET_ARP_CARRY (sizeof(ethHeader))
#define NET_ARP(packet) ((arpPacket *)((size_t)(packet) + NET_ARP_CARRY))

// todo: add a hashmap to improve performance
typedef struct arpEntry {
  LLheader _ll;

  uint8_t mac[MAC_BYTE_SIZE];
  uint8_t ip[IPv4_BYTE_SIZE];
} arpEntry;

typedef struct arpRequest {
  uint8_t ip[IPv4_BYTE_SIZE];
} arpRequest;

typedef struct arpStore {
  LLcontrol table; // struct arpEntry
  Spinlock  LOCK_LL_TABLE;
} arpStore;

void netArpHandle(void *_nic, void *packet, uint32_t size);
bool netArpTranslate(void *_nic, uint8_t *ip, uint8_t *res);

#endif
