#include <SDL2/SDL.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

int quit = 0;

// Global buffer for the SDL callback to read from
uint8_t *audio_pos;
uint32_t audio_len;

void audio_callback(void *userdata, Uint8 *stream, int len) {
  // log_message("Audio callback called with %d bytes\n", len);

  SDL_memset(stream, 0, len);
  if (audio_len == 0)
    return;

  len = (len > audio_len ? audio_len : len);

  // Mix the decoded data into the SDL output stream
  SDL_MixAudio(stream, audio_pos, len, SDL_MIX_MAXVOLUME);
  audio_pos += len;
  audio_len -= len;
}

int main(int argc, char *argv[]) {
  if (argc < 2)
    return -1;

  // 1. Initialize FFmpeg
  AVFormatContext *format_ctx = avformat_alloc_context();
  avformat_open_input(&format_ctx, argv[1], NULL, NULL);
  avformat_find_stream_info(format_ctx, NULL);

  // Find the first audio stream
  int audio_stream = -1;
  for (int i = 0; i < format_ctx->nb_streams; i++) {
    if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
      audio_stream = i;
      break;
    }
  }

  // 2. Setup Decoder
  AVCodecParameters *params = format_ctx->streams[audio_stream]->codecpar;
  const AVCodec *codec = avcodec_find_decoder(params->codec_id);
  AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
  avcodec_parameters_to_context(codec_ctx, params);
  avcodec_open2(codec_ctx, codec, NULL);

  // 3. Setup SDL Audio (S16SYS)
  SDL_Init(SDL_INIT_AUDIO);
  SDL_AudioSpec want;
  want.freq = codec_ctx->sample_rate;
  want.format = AUDIO_S16SYS;
  want.channels = codec_ctx->ch_layout.nb_channels;
  want.samples = 1024;
  want.callback = audio_callback;

  SDL_OpenAudio(&want, NULL);
  SDL_PauseAudio(0); // Unpause immediately

  // 4. Setup Resampler (Convert any format to S16)
  struct SwrContext *swr = swr_alloc();
  swr_alloc_set_opts2(&swr, &codec_ctx->ch_layout, AV_SAMPLE_FMT_S16,
                      codec_ctx->sample_rate, &codec_ctx->ch_layout,
                      codec_ctx->sample_fmt, codec_ctx->sample_rate, 0, NULL);
  swr_init(swr);

  // listen to SDL events
  SDL_Event event;

  // 5. Main Decoding Loop
  AVPacket *pkt = av_packet_alloc();
  AVFrame *frame = av_frame_alloc();
  uint8_t *output_buffer = NULL;

  while (av_read_frame(format_ctx, pkt) >= 0) {
    if (pkt->stream_index == audio_stream) {
      avcodec_send_packet(codec_ctx, pkt);
      while (avcodec_receive_frame(codec_ctx, frame) == 0) {
        // Convert to S16
        int out_samples = frame->nb_samples;
        av_samples_alloc(&output_buffer, NULL, codec_ctx->ch_layout.nb_channels,
                         out_samples, AV_SAMPLE_FMT_S16, 0);
        swr_convert(swr, &output_buffer, out_samples,
                    (const uint8_t **)frame->data, frame->nb_samples);

        // Set globals for SDL callback
        audio_pos = output_buffer;
        audio_len = out_samples * codec_ctx->ch_layout.nb_channels * 2;

        // Wait until SDL consumes the buffer (Simple sync)
        while (audio_len > 0)
          SDL_Delay(10);

        av_freep(&output_buffer);
      }
    }
    av_packet_unref(pkt);
  }

  while (!quit) {
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        quit = 1;
      }
    }
  }

  // Cleanup
  SDL_CloseAudio();
  avcodec_free_context(&codec_ctx);
  swr_free(&swr);
  avformat_close_input(&format_ctx);
  av_frame_free(&frame);
  av_packet_free(&pkt);
  SDL_Quit();
  return 0;
}
