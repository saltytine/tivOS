#include <arp.h>
#include <eth.h>
#include <ipv4.h>
#include <nic_controller.h>

// Layer2: Raw ethernet packets

uint8_t macBroadcast[MAC_BYTE_SIZE] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
uint8_t macZero[MAC_BYTE_SIZE] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t addressNull[IPv4_BYTE_SIZE] = {0x00, 0x00, 0x00, 0x00};
uint8_t addressBroadcast[IPv4_BYTE_SIZE] = {0xff, 0xff, 0xff, 0xff};

void dbgMac(uint8_t *mac) {
  debugf("%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3],
         mac[4], mac[5]);
}

void dbgIp(uint8_t *ip) { debugf("%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]); }

void netEthHandle(void *_nic, void *packet, uint32_t size) {
  NIC *nic = _nic;
  if (size < sizeof(ethHeader)) {
    debugf("[net::eth] Drop: Too small\n");
    return;
  }

  ethHeader *eth = (ethHeader *)packet;
  if (memcmp(eth->dest, nic->MAC, MAC_BYTE_SIZE) != 0 &&
      memcmp(eth->dest, macBroadcast, MAC_BYTE_SIZE) &&
      memcmp(eth->dest, macZero, MAC_BYTE_SIZE)) {
    debugf("[net::eth] Drop: Neither ours nor broadcast\n");
    return;
  }

  switch (switch_endian_16(eth->type)) {
  case NET_ETHERTYPE_ARP:
    netArpHandle(_nic, packet, size);
    break;
  case NET_ETHERTYPE_IPV4:
    netIPv4Handle(_nic, packet, size);
    break;
  case NET_ETHERTYPE_IPV6:
    // no, just no
    break;
  default:
    debugf("[net::eth] Drop: Unhandled ethertype\n");
    break;
  }
}

void netEthSend(void *_nic, void *packet, uint32_t size, uint8_t *targetMac,
                uint16_t ethertype) {
  NIC *nic = _nic;
  assert(size >= sizeof(ethHeader));
  assert(size <= nic->mtu); // last assert in the chain
  ethHeader *eth = (ethHeader *)packet;
  memcpy(eth->src, nic->MAC, MAC_BYTE_SIZE);
  memcpy(eth->dest, targetMac, MAC_BYTE_SIZE);
  eth->type = switch_endian_16(ethertype);
  sendPacketRaw(nic, packet, size);
}
