#ifndef PACKET_QUEUE_H
#define PACKET_QUEUE_H

#include <SDL2/SDL.h>
#include <libavcodec/packet.h>

typedef struct CPacketList {
  AVPacket pkt;
  struct CPacketList *next;
} CPacketList;

typedef struct PacketQueue {
  CPacketList *first_pkt, *last_pkt;
  int nb_packets;
  int size;
  SDL_mutex *mutex;
  SDL_cond *cond;
} PacketQueue;

void packet_queue_init(PacketQueue *q);
void packet_queue_abort(PacketQueue *q);
int packet_queue_put(PacketQueue *q, AVPacket *pkt);
int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block);
int packet_queue_destroy(PacketQueue *q);

#endif