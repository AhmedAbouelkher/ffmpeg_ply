#include "player_audio.h"
#include <SDL2/SDL.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <stdio.h>

extern int quit;

// MARK:- Audio State

AudioState *audio_state_init(AVCodecContext *codecCtx) {
  AudioState *is = av_mallocz(sizeof(AudioState));
  packet_queue_init(&is->queue);
  is->codec_ctx = codecCtx;
  is->frame = av_frame_alloc();
  // Resampler: Source -> S16 Mono/Stereo
  swr_alloc_set_opts2(&is->swr_ctx, &codecCtx->ch_layout, AV_SAMPLE_FMT_S16,
                      codecCtx->sample_rate, &codecCtx->ch_layout,
                      codecCtx->sample_fmt, codecCtx->sample_rate, 0, NULL);
  swr_init(is->swr_ctx);
  return is;
}

int audio_state_destroy(AudioState *is) {
  if (!is) {
    return -1;
  }
  packet_queue_destroy(&is->queue);
  av_frame_free(&is->frame);
  swr_free(&is->swr_ctx);
  av_free(is);
  return 0;
}

// MARK:- Callback

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
  SDL_memset(stream, 0, len);
  if (quit)
    return;

  AudioState *is = (AudioState *)userdata;
  int leftLen, audio_size;

  while (len > 0) {
    if (is->audio_buf_index >= is->audio_buf_size) {
      // We've consumed the current buffer, decode more
      audio_size = decode_audio_frame(is);
      if (audio_size < 0) {
        // If error or quit, output silence
        is->audio_buf_size = AUDIO_BUFFER_SIZE;
        memset(is->audio_buf, 0, is->audio_buf_size);
      } else {
        is->audio_buf_size = audio_size;
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
