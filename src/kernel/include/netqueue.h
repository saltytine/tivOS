#include "linked_list.h"
#include "util.h"

#ifndef NET_QUEUE_H
#define NET_QUEUE_H

typedef void (*QueueItemCallback)(void *item);
typedef enum { NET_QUEUE_ARP } netQueueComp;

typedef struct netQueueItem {
  LLheader _ll;

  void  *nic;
  void  *task;
  size_t unixTime;

  // size_t pollWakeupKey;
  // size_t pollWakeupEvent;

  netQueueComp      component;
  QueueItemCallback callback;

  void *dir;
} netQueueItem;

LLcontrol dsNetQueue; // struct netQueueItem
Spinlock  LOCK_LL_NET_QUEUE;

netQueueItem *netQueueAllocateM(netQueueComp component, void *nic);

void helperNetQueue();

#endif
