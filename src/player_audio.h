#ifndef PLAYER_AUDIO_H
#define PLAYER_AUDIO_H

#include "packet_queue.h"
#include <SDL2/SDL.h>
#include <libavcodec/avcodec.h>

#define AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE 192000

typedef struct AudioState {
  PacketQueue queue;
  AVCodecContext *codec_ctx;
  struct SwrContext *swr_ctx;

  // Internal buffer for decoded samples
  uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE * 3 / 2) * 2];
  unsigned int audio_buf_size;
  unsigned int audio_buf_index;
  AVFrame *frame;
} AudioState;

AudioState *audio_state_init(AVCodecContext *codecCtx);
int audio_state_destroy(AudioState *is);

void audio_callback(void *userdata, uint8_t *stream, int len);

#endif
