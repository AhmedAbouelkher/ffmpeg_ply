#include "packet_queue.h"
#include <stdlib.h>
#include <string.h>

extern int quit;

void packet_queue_init(PacketQueue *q) {
  memset(q, 0, sizeof(PacketQueue));
  q->mutex = SDL_CreateMutex();
  q->cond = SDL_CreateCond();
}

// [will cause memory leak if not destroyed]
void packet_queue_abort(PacketQueue *q) {
  SDL_LockMutex(q->mutex);
  quit = 1;
  SDL_CondBroadcast(q->cond);
  SDL_UnlockMutex(q->mutex);
}

int packet_queue_destroy(PacketQueue *q) {
  if (!q) {
    return -1;
  }
  CPacketList *pkt1, *pkt2;
  SDL_LockMutex(q->mutex);
  pkt1 = q->first_pkt;
  while (pkt1) {
    pkt2 = pkt1;
    pkt1 = pkt1->next;
    av_packet_unref(&pkt2->pkt);
    free(pkt2);
  }
  SDL_UnlockMutex(q->mutex);
  SDL_DestroyMutex(q->mutex);
  SDL_DestroyCond(q->cond);
  return 0;
}

int packet_queue_put(PacketQueue *q, AVPacket *pkt) {
  CPacketList *new_pkt = malloc(sizeof(CPacketList));
  if (!new_pkt) {
    return -1;
  }
  new_pkt->pkt = *pkt;
  new_pkt->next = NULL;
  SDL_LockMutex(q->mutex);
  if (!q->last_pkt) {
    q->first_pkt = new_pkt;
  } else {
    q->last_pkt->next = new_pkt;
  }
  q->last_pkt = new_pkt;
  q->nb_packets++;
  q->size += pkt->size;
  SDL_CondSignal(q->cond);
  SDL_UnlockMutex(q->mutex);
  return 0;
}

int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block) {
  CPacketList *pkt1;
  int ret;
  SDL_LockMutex(q->mutex);
  for (;;) {
    if (quit) {
      ret = -1;
      break;
    }
    pkt1 = q->first_pkt;
    if (pkt1) {
      // Remove the first packet from the queue by advancing the head pointer
      q->first_pkt = pkt1->next;
      // If the queue is now empty, also clear the tail pointer
      if (!q->first_pkt)
        q->last_pkt = NULL;
      // Decrement the packet count
      q->nb_packets--;
      // Subtract this packet's size from the total queue size
      q->size -= pkt1->pkt.size;
      // Copy the packet data to the output parameter
      *pkt = pkt1->pkt;
      // Free the packet list node (but not the packet data itself, which was
      // copied)
      free(pkt1);
      ret = 1;
      break;
    } else if (!block) {
      ret = 0;
      break;
    } else {
      SDL_CondWait(q->cond, q->mutex);
    }
  }
  SDL_UnlockMutex(q->mutex);
  return ret;
}
