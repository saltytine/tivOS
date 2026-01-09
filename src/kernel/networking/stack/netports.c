#include <netports.h>
#include <nic_controller.h>
#include <poll.h>

// Free port management utility for use by TCP & UDP

bool netPortsIsAllocated(NetPorts *netports, uint16_t port) {
  if (port < 8) // because of the 0xFF'd first block
    return false;
  spinlockAcquire(&netports->LOCK_MAP_PORTS);
  bool allocated = bitmapGenericGet(netports->ports, port);
  spinlockRelease(&netports->LOCK_MAP_PORTS);
  return allocated;
}

void netPortsFree(NetPorts *netports, uint16_t localPort) {
  assert(localPort);
  spinlockAcquire(&netports->LOCK_MAP_PORTS);
  assert(bitmapGenericGet(netports->ports, localPort));
  bitmapGenericSet(netports->ports, localPort, false);
  spinlockRelease(&netports->LOCK_MAP_PORTS);
}

uint16_t netPortsGenInner(NetPorts *netports, int i) {
  bitmapGenericSet(netports->ports, i, true);
  netports->lastEphPort = i;
  spinlockRelease(&netports->LOCK_MAP_PORTS);
  return i;
}

uint16_t netPortsGen(NetPorts *netports) {
  spinlockAcquire(&netports->LOCK_MAP_PORTS);
  netports->lastEphPort++;
  if (netports->lastEphPort < NETPORTS_EPH_START)
    netports->lastEphPort = NETPORTS_EPH_START + rand() % 128;
  for (int i = netports->lastEphPort; i < NETPORTS_AVAILABLE_PORTS; i++) {
    if (!bitmapGenericGet(netports->ports, i))
      return netPortsGenInner(netports, i);
  }

  for (int i = NETPORTS_EPH_START; i < netports->lastEphPort; i++) {
    if (!bitmapGenericGet(netports->ports, i))
      return netPortsGenInner(netports, i);
  }

  // you opened too many connections, face the consequences
  assert(false);
  return -1;
}

// returns false when it already exists
bool netPortsMarkSafe(NetPorts *netports, uint16_t localPort) {
  spinlockAcquire(&netports->LOCK_MAP_PORTS);
  bool existing = bitmapGenericGet(netports->ports, localPort);
  if (!existing)
    bitmapGenericSet(netports->ports, localPort, true);
  spinlockRelease(&netports->LOCK_MAP_PORTS);
  return !existing;
}
