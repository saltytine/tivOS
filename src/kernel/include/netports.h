#include "linked_list.h"
#include "util.h"

#ifndef NET_PORTS_H
#define NET_PORTS_H

#define NETPORTS_AVAILABLE_PORTS 65536
#define NETPORTS_EPH_START 49152

typedef struct NetPorts {
  uint8_t  ports[NETPORTS_AVAILABLE_PORTS / 8]; // bitmap
  uint16_t lastEphPort;
  Spinlock LOCK_MAP_PORTS;
} NetPorts;

bool     netPortsIsAllocated(NetPorts *netports, uint16_t port);
uint16_t netPortsGen(NetPorts *netports);
bool     netPortsMarkSafe(NetPorts *netports, uint16_t localPort);
void     netPortsFree(NetPorts *netports, uint16_t localPort);

#endif
