#include <arp.h>
#include <netqueue.h>
#include <nic_controller.h>
#include <timer.h>

// Layer3: Address Resolution Protocol implementation

bool netArpTableLookupCb(void *data, void *ctx) {
  arpEntry *entry = data;
  uint8_t  *against = ctx;
  return memcmp(entry->ip, against, IPv4_BYTE_SIZE) == 0;
}

bool netArpTableLookupMacCb(void *data, void *ctx) {
  arpEntry *entry = data;
  uint8_t  *against = ctx;
  return memcmp(entry->mac, against, MAC_BYTE_SIZE) == 0;
}

bool netArpTableLookup(NIC *nic, uint8_t *ip, uint8_t *res) {
  spinlockAcquire(&nic->arp.LOCK_LL_TABLE);
  arpEntry *entry = LinkedListSearch(&nic->arp.table, netArpTableLookupCb, ip);
  if (!entry) {
    spinlockRelease(&nic->arp.LOCK_LL_TABLE);
    return false;
  }

  memcpy(res, entry->mac, MAC_BYTE_SIZE);
  spinlockRelease(&nic->arp.LOCK_LL_TABLE);
  return true;
}

void netArpTableAdd(NIC *nic, uint8_t *mac, uint8_t *ip) {
  spinlockAcquire(&nic->arp.LOCK_LL_TABLE);
  arpEntry *existingIp =
      LinkedListSearch(&nic->arp.table, netArpTableLookupCb, ip);
  arpEntry *existingMac =
      LinkedListSearch(&nic->arp.table, netArpTableLookupMacCb, mac);

  if (existingIp && existingMac && existingIp != existingMac) {
    // free one of them if both exist
    LinkedListRemove(&nic->arp.table, sizeof(arpEntry), existingMac);
    existingMac = 0;
  }

  arpEntry *entry = 0;
  if (existingIp)
    entry = existingIp;
  else if (existingMac)
    entry = existingMac;
  else
    entry = LinkedListAllocate(&nic->arp.table, sizeof(arpEntry));

  memcpy(entry->mac, mac, MAC_BYTE_SIZE);
  memcpy(entry->ip, ip, IPv4_BYTE_SIZE);
  spinlockRelease(&nic->arp.LOCK_LL_TABLE);
}

force_inline void netArpReply(NIC *nic, uint8_t *reqMac, uint8_t *reqIp) {
  int   size = NET_ARP_CARRY + sizeof(arpPacket);
  void *packet = calloc(size, 1);

  arpPacket *arp = NET_ARP(packet);
  arp->hardwareType = switch_endian_16(ARP_HARDWARE_TYPE);
  arp->protocolType = switch_endian_16(ARP_PROTOCOL_TYPE);
  arp->hardwareSize = MAC_BYTE_SIZE;
  arp->protocolSize = IPv4_BYTE_SIZE;
  arp->opcode = switch_endian_16(ARP_OP_REPLY);

  memcpy(arp->targetMac, reqMac, MAC_BYTE_SIZE);
  memcpy(arp->targetIp, reqIp, IPv4_BYTE_SIZE);

  memcpy(arp->senderMac, nic->MAC, MAC_BYTE_SIZE);
  memcpy(arp->senderIp, nic->ip, IPv4_BYTE_SIZE);

  netEthSend(nic, packet, size, reqMac, ETHERTYPE_ARP);
  free(packet);
}

bool netArpQueueCb(void *data, void *ctx) {
  netQueueItem *item = data;
  uint8_t      *ip = ctx;
  if (item->component != NET_QUEUE_ARP)
    return false;
  arpRequest *request = item->dir;
  return memcmp(request->ip, ip, IPv4_BYTE_SIZE) == 0;
}

void netArpHandle(void *_nic, void *packet, uint32_t size) {
  NIC *nic = _nic;
  if (size < NET_ARP_CARRY + sizeof(arpPacket)) {
    debugf("[net::arp] Drop: Too small\n");
    return;
  }

  arpPacket *arp = NET_ARP(packet);
  if (switch_endian_16(arp->hardwareType) != ARP_HARDWARE_TYPE ||
      switch_endian_16(arp->protocolType) != ARP_PROTOCOL_TYPE ||
      arp->hardwareSize != MAC_BYTE_SIZE ||
      arp->protocolSize != IPv4_BYTE_SIZE) {
    debugf("[net::arp] Drop: Invalid hw/proto type or hw size\n");
    return;
  }

  if (memcmp(arp->senderMac, ((ethHeader *)packet)->src, MAC_BYTE_SIZE) != 0) {
    debugf("[net::arp] Drop: MAC sender doesn't match w/eth sender\n");
    return;
  }

  switch (switch_endian_16(arp->opcode)) {
  case ARP_OP_REQUEST:
    if (memcmp(arp->targetIp, addressNull, IPv4_BYTE_SIZE) == 0) {
      debugf("[net::arp] Drop: Asking about null address\n");
      break;
    }
    if (memcmp(arp->targetIp, nic->ip, IPv4_BYTE_SIZE) == 0)
      netArpReply(nic, arp->senderMac, arp->senderIp);
    break;
  case ARP_OP_REPLY:
    if (memcmp(arp->senderIp, addressNull, IPv4_BYTE_SIZE) == 0) {
      debugf("[net::arp] Drop: Came back to us with a null address\n");
      break;
    }

    spinlockAcquire(&LOCK_LL_NET_QUEUE);
    netQueueItem *target =
        LinkedListSearch(&dsNetQueue, netArpQueueCb, arp->senderIp);
    bool askedFor = target;
    if (target) {
      assert(target->dir);
      free(target->dir);
      assert(LinkedListRemove(&dsNetQueue, sizeof(netQueueItem), target));
    }
    spinlockRelease(&LOCK_LL_NET_QUEUE);

    if (askedFor)
      netArpTableAdd(nic, arp->senderMac, arp->senderIp);
    else
      debugf("[net::arp] Drop: We didn't ask for this response\n");
    break;
  default:
    debugf("[net::arp] Drop: Invalid opcode\n");
    break;
  }
}

bool netArpTranslate(void *_nic, uint8_t *ip, uint8_t *res) {
  bool ret = netArpTableLookup(_nic, ip, res);
  if (ret)
    return ret;

  NIC *nic = _nic;

  // prepare the queue
  spinlockAcquire(&LOCK_LL_NET_QUEUE);
  netQueueItem *item = LinkedListSearch(&dsNetQueue, netArpQueueCb, ip);
  bool          proceed = true;
  if (!item) {
    item = netQueueAllocateM(NET_QUEUE_ARP, _nic);
    item->callback = 0;

    arpRequest *request = malloc(sizeof(arpRequest)); // !
    memcpy(request->ip, ip, IPv4_BYTE_SIZE);
    item->dir = request;
  } else {
    proceed = UNIX_TIME_PASSED(item->unixTime, 500);
    if (proceed)
      item->unixTime = timerTicks;
  }
  spinlockRelease(&LOCK_LL_NET_QUEUE);

  // already going to be found
  if (!proceed)
    return false;

  // send the packet
  int   size = NET_ARP_CARRY + sizeof(arpPacket);
  void *packet = calloc(size, 1);

  arpPacket *arp = NET_ARP(packet);
  arp->hardwareType = switch_endian_16(ARP_HARDWARE_TYPE);
  arp->protocolType = switch_endian_16(ARP_PROTOCOL_TYPE);
  arp->hardwareSize = MAC_BYTE_SIZE;
  arp->protocolSize = IPv4_BYTE_SIZE;
  arp->opcode = switch_endian_16(ARP_OP_REQUEST);

  // memcpy(arp->targetMac, targetMac, MAC_BYTE_SIZE);
  memcpy(arp->targetIp, ip, IPv4_BYTE_SIZE);

  memcpy(arp->senderMac, nic->MAC, MAC_BYTE_SIZE);
  memcpy(arp->senderIp, nic->ip, IPv4_BYTE_SIZE);

  netEthSend(nic, packet, size, macBroadcast, ETHERTYPE_ARP);
  free(packet);

  return false;
}
