#include "libavcodec/codec_par.h"
#include "libavcodec/packet.h"
#include "libavutil/avutil.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_main.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/codec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "logger.h"
#include "packet_queue.h"
#include "player_audio.h"

int quit = 0;

int main(int argc, char *argv[]) {
  if (argc != 2) {
    log_message_ln("Usage: %s <input file>", argv[0]);
    return -1;
  }

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
    log_message_ln("SDL_Init Error: %s", SDL_GetError());
    return 1;
  }

  AVFormatContext *formatContext = NULL;

  AVCodecContext *videoCodecContext = NULL;
  const AVCodec *videoCodec = NULL;
  Uint8 *yPlane, *uPlane, *vPlane;

  AVCodecContext *audioCodecContext = NULL;
  const AVCodec *audioCodec = NULL;

  SDL_AudioSpec wanted_spec;

  // MARK:- Format context setup

  if (avformat_open_input(&formatContext, argv[1], NULL, NULL) != 0) {
    log_message_ln("Failed to open input file");
    return -1;
  }
  if (avformat_find_stream_info(formatContext, NULL) < 0) {
    log_message_ln("Failed to find stream info");
    return -1;
  }

  // av_dump_format(formatContext, 0, argv[1], 0);

  // get the video file name without any path or extension
  const char *videoFileName = strrchr(argv[1], '/');
  if (videoFileName) {
    videoFileName++;
  } else {
    videoFileName = argv[1];
  }

  // ------------------------------------------------------------------------ //

  // MARK:- Stream setup
  int videoStreamIndex = -1;
  int audioStreamIndex = -1;
  for (int i = 0; i < formatContext->nb_streams; i++) {
    enum AVMediaType mediaType =
        formatContext->streams[i]->codecpar->codec_type;
    if (mediaType == AVMEDIA_TYPE_VIDEO && videoStreamIndex < 0) {
      videoStreamIndex = i;
    } else if (mediaType == AVMEDIA_TYPE_AUDIO && audioStreamIndex < 0) {
      audioStreamIndex = i;
    }
  }
  if (videoStreamIndex == -1) {
    log_message_ln("No video stream found");
    return -1;
  }
  if (audioStreamIndex == -1) {
    log_message_ln("No audio stream found");
    return -1;
  }

  // ------------------------------------------------------------------------ //

  // MARK:- Video codec setup
  videoCodec = avcodec_find_decoder(
      formatContext->streams[videoStreamIndex]->codecpar->codec_id);
  if (!videoCodec) {
    log_message_ln("Unsupported codec");
    return -1;
  }
  videoCodecContext = avcodec_alloc_context3(videoCodec);
  if (!videoCodecContext) {
    log_message_ln("Failed to allocate codec context");
    return -1;
  }
  if (avcodec_parameters_to_context(
          videoCodecContext,
          formatContext->streams[videoStreamIndex]->codecpar) < 0) {
    log_message_ln("Failed to copy codec parameters to context");
    return -1;
  }
  if (avcodec_open2(videoCodecContext, videoCodec, NULL) < 0) {
    log_message_ln("Failed to open codec");
    return -1;
  }

  // ------------------------------------------------------------------------ //

  // MARK:- Audio codec setup
  AVCodecParameters *audioCodecParams =
      formatContext->streams[audioStreamIndex]->codecpar;
  audioCodec = avcodec_find_decoder(audioCodecParams->codec_id);
  if (!audioCodec) {
    log_message_ln("Unsupported audio codec");
    return -1;
  }
  audioCodecContext = avcodec_alloc_context3(audioCodec);
  if (!audioCodecContext) {
    log_message_ln("Failed to allocate audio codec context");
    return -1;
  }
  if (avcodec_parameters_to_context(audioCodecContext, audioCodecParams) < 0) {
    log_message_ln("Failed to copy audio codec parameters to context");
    return -1;
  }
  if (avcodec_open2(audioCodecContext, audioCodec, NULL) < 0) {
    log_message_ln("Failed to open audio codec");
    return -1;
  }

  // ------------------------------------------------------------------------ //
  AudioState *is = audio_state_init(audioCodecContext);
  if (!is) {
    log_message_ln("Failed to initialize audio state");
    return -1;
  }

  wanted_spec.freq = audioCodecContext->sample_rate;
  wanted_spec.format = AUDIO_S16SYS;
  wanted_spec.channels = audioCodecContext->ch_layout.nb_channels;
  wanted_spec.silence = 0;
  wanted_spec.samples = AUDIO_BUFFER_SIZE;
  wanted_spec.callback = audio_callback;
  wanted_spec.userdata = is;

  if (SDL_OpenAudio(&wanted_spec, NULL) < 0) {
    log_message_ln("Failed to open audio: %s", SDL_GetError());
    return -1;
  }
  SDL_PauseAudio(0);

  // ------------------------------------------------------------------------ //

  // MARK:- Video Frame setup

  AVFrame *videoFrame = av_frame_alloc();

  SDL_Window *window = SDL_CreateWindow(
      videoFileName, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      videoCodecContext->width, videoCodecContext->height, 0);
  if (!window) {
    log_message_ln("Failed to create window: %s", SDL_GetError());
    return -1;
  }

  SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
  if (!renderer) {
    log_message_ln("Failed to create renderer: %s", SDL_GetError());
    return -1;
  }

  // create a YUV texture
  SDL_Texture *texture = SDL_CreateTexture(
      renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING,
      videoCodecContext->width, videoCodecContext->height);
  if (!texture) {
    log_message_ln("Failed to create texture: %s", SDL_GetError());
    return -1;
  }

  struct SwsContext *swsContext =
      sws_getContext(videoCodecContext->width, videoCodecContext->height,
                     videoCodecContext->pix_fmt, videoCodecContext->width,
                     videoCodecContext->height, AV_PIX_FMT_YUV420P,
                     SWS_BILINEAR, NULL, NULL, NULL);
  if (!swsContext) {
    log_message_ln("Failed to get context");
    return -1;
  }

  AVPacket pkt;
  int uvPlanePitch = videoCodecContext->width / 2;
  size_t yPlaneSize = videoCodecContext->width * videoCodecContext->height;
  size_t uvPlaneSize = yPlaneSize / 4;
  yPlane = (Uint8 *)malloc(yPlaneSize);
  uPlane = (Uint8 *)malloc(uvPlaneSize);
  vPlane = (Uint8 *)malloc(uvPlaneSize);
  if (!yPlane || !uPlane || !vPlane) {
    log_message_ln("Failed to allocate memory for planes");
    return -1;
  }

  double frameDelay =
      av_q2d(formatContext->streams[videoStreamIndex]->avg_frame_rate);
  if (frameDelay > 0) {
    frameDelay = 1.0 / frameDelay * 1000.0;
  } else {
    frameDelay = 40.0;
  }
  log_message_ln("Video frame delay: %f", frameDelay);

  SDL_Event event;

  while (av_read_frame(formatContext, &pkt) >= 0 && !quit) {
    if (pkt.stream_index == videoStreamIndex) {
      if (avcodec_send_packet(videoCodecContext, &pkt) < 0) {
        printf("Failed to send packet\n");
        av_packet_unref(&pkt);
        continue;
      }
      if (avcodec_receive_frame(videoCodecContext, videoFrame) < 0) {
        av_packet_unref(&pkt);
        continue;
      }

      AVFrame frameYUV;
      frameYUV.data[0] = yPlane;
      frameYUV.data[1] = uPlane;
      frameYUV.data[2] = vPlane;
      frameYUV.linesize[0] = videoCodecContext->width;
      frameYUV.linesize[1] = uvPlanePitch;
      frameYUV.linesize[2] = uvPlanePitch;

      sws_scale(swsContext, (uint8_t const *const *)videoFrame->data,
                videoFrame->linesize, 0, videoCodecContext->height,
                frameYUV.data, frameYUV.linesize);

      SDL_UpdateYUVTexture(texture, NULL, yPlane, videoCodecContext->width,
                           uPlane, uvPlanePitch, vPlane, uvPlanePitch);

      SDL_RenderClear(renderer);
      SDL_RenderCopy(renderer, texture, NULL, NULL);
      SDL_RenderPresent(renderer);

      av_packet_unref(&pkt);

    } else if (pkt.stream_index == audioStreamIndex) {
      packet_queue_put(&is->queue, &pkt);
    } else {
      av_packet_unref(&pkt);
    }

    // SDL_Delay((Uint32)frameDelay);

    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        packet_queue_abort(&is->queue);
        quit = 1;
        break;
      }
    }
  }

  while (!quit) {
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        packet_queue_abort(&is->queue);
        quit = 1;
        break;
      }
    }
    SDL_Delay(100);
  }

  av_frame_free(&videoFrame);
  free(yPlane);
  free(uPlane);
  free(vPlane);
  sws_freeContext(swsContext);

  audio_state_destroy(is);

  avcodec_free_context(&videoCodecContext);
  avcodec_free_context(&audioCodecContext);
  avformat_close_input(&formatContext);

  SDL_CloseAudio();
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  log_message_ln("Application ended successfully");

  return 0;
}