#include <SDL2/SDL.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

int quit = 0;

void log_message_ln(const char *fmt, ...);

// --- YOUR QUEUE STRUCTURES ---
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

void packet_queue_init(PacketQueue *q) {
  memset(q, 0, sizeof(PacketQueue));
  q->mutex = SDL_CreateMutex();
  q->cond = SDL_CreateCond();
}

void packet_queue_abort(PacketQueue *q) {
  SDL_LockMutex(q->mutex);
  quit = 1;
  SDL_CondBroadcast(q->cond);
  SDL_UnlockMutex(q->mutex);
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

int packet_queue_destroy(PacketQueue *q) {
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

typedef struct AudioState {
  PacketQueue queue;
  AVCodecContext *codec_ctx;
  struct SwrContext *swr_ctx;

  // Internal buffer for decoded samples
  uint8_t audio_buf[(192000 * 3 / 2) * 2];
  unsigned int audio_buf_size;
  unsigned int audio_buf_index;
  AVFrame *frame;
} AudioState;

// Decodes one packet from the queue and returns the number of bytes produced
int decode_audio_frame(AudioState *is) {
  AVPacket pkt;
  int data_size = 0;

  if (packet_queue_get(&is->queue, &pkt, 1) < 0)
    return -1;

  if (avcodec_send_packet(is->codec_ctx, &pkt) == 0) {
    while (avcodec_receive_frame(is->codec_ctx, is->frame) == 0) {
      // Convert to S16 format
      int out_samples = swr_get_out_samples(is->swr_ctx, is->frame->nb_samples);
      uint8_t *out[] = {is->audio_buf};

      int len =
          swr_convert(is->swr_ctx, out, out_samples,
                      (const uint8_t **)is->frame->data, is->frame->nb_samples);

      if (len < 0) {
        av_frame_unref(is->frame);
        continue;
      }

      data_size = len * is->codec_ctx->ch_layout.nb_channels *
                  av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);

      av_frame_unref(is->frame);
      av_packet_unref(&pkt);
      return data_size; // Return size of the resampled data in audio_buf
    }
  }
  av_packet_unref(&pkt);
  return -1;
}

void audio_callback(void *userdata, Uint8 *stream, int len) {
  AudioState *is = (AudioState *)userdata;
  int leftLen, audio_size;

  while (len > 0 && !quit) {
    if (is->audio_buf_index >= is->audio_buf_size) {
      // We've consumed the current buffer, decode more
      audio_size = decode_audio_frame(is);
      if (audio_size < 0) {
        // If error or quit, output silence
        is->audio_buf_size = 1024;
        memset(is->audio_buf, 0, is->audio_buf_size);
        log_message_ln("We have an error in the audio callback");
      } else {
        is->audio_buf_size = audio_size;
        log_message_ln(
            "We have decoded %d bytes of audio for request of %d bytes",
            audio_size, len);
      }
      is->audio_buf_index = 0;
    }

    leftLen = is->audio_buf_size - is->audio_buf_index;
    if (leftLen > len)
      leftLen = len;

    // Copy directly to stream (No SDL_MixAudio)
    memcpy(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, leftLen);

    len -= leftLen;
    stream += leftLen;
    is->audio_buf_index += leftLen;
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2)
    return -1;

  // FFmpeg setup
  AVFormatContext *fmt_ctx = NULL;
  avformat_open_input(&fmt_ctx, argv[1], NULL, NULL);
  avformat_find_stream_info(fmt_ctx, NULL);

  int stream_idx =
      av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
  AVCodecParameters *params = fmt_ctx->streams[stream_idx]->codecpar;
  const AVCodec *codec = avcodec_find_decoder(params->codec_id);
  AVCodecContext *c_ctx = avcodec_alloc_context3(codec);
  avcodec_parameters_to_context(c_ctx, params);
  avcodec_open2(c_ctx, codec, NULL);

  // State initialization
  AudioState *is = av_mallocz(sizeof(AudioState));
  packet_queue_init(&is->queue);
  is->codec_ctx = c_ctx;
  is->frame = av_frame_alloc();

  // Resampler: Source -> S16 Mono/Stereo
  swr_alloc_set_opts2(&is->swr_ctx, &c_ctx->ch_layout, AV_SAMPLE_FMT_S16,
                      c_ctx->sample_rate, &c_ctx->ch_layout, c_ctx->sample_fmt,
                      c_ctx->sample_rate, 0, NULL);
  swr_init(is->swr_ctx);

  // SDL Setup
  SDL_Init(SDL_INIT_AUDIO);
  SDL_AudioSpec want;
  want.freq = c_ctx->sample_rate;
  want.format = AUDIO_S16SYS;
  want.channels = c_ctx->ch_layout.nb_channels;
  want.samples = 1024;
  want.callback = audio_callback;
  want.userdata = is;

  if (SDL_OpenAudio(&want, NULL) < 0)
    return -1;
  SDL_PauseAudio(0);

  // Main demux loop
  AVPacket pkt;
  while (av_read_frame(fmt_ctx, &pkt) >= 0) {
    if (pkt.stream_index == stream_idx) {
      packet_queue_put(&is->queue, &pkt);
    } else {
      av_packet_unref(&pkt);
    }
    if (quit)
      break;
  }

  SDL_Delay(3000);

  // packet_queue_destroy(&is->queue);
  av_free(is->frame);
  av_free(is);

  avcodec_free_context(&c_ctx);
  avformat_close_input(&fmt_ctx);
  SDL_CloseAudio();
  SDL_Quit();

  return 0;
}

void log_message_ln(const char *fmt, ...) {
  struct timeval tv;
  gettimeofday(&tv, NULL);

  struct tm *tm_info = localtime(&tv.tv_sec);

  char time_buf[30];
  strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

  printf("[%s.%03d] ", time_buf, (int)(tv.tv_usec / 1000));

  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
  printf("\n");
}