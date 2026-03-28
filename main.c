#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/codec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <stdint.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: %s <input file>\n", argv[0]);
    return -1;
  }

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("SDL_Init Error: %s", SDL_GetError());
    return 1;
  }

  AVFormatContext *formatContext = NULL;
  AVCodecContext *codecContext = NULL;
  const AVCodec *codec = NULL;
  Uint8 *yPlane, *uPlane, *vPlane;

  if (avformat_open_input(&formatContext, argv[1], NULL, NULL) != 0) {
    printf("Failed to open input file\n");
    return -1;
  }

  if (avformat_find_stream_info(formatContext, NULL) < 0) {
    printf("Failed to find stream info\n");
    return -1;
  }

  int videoStreamIndex = -1;
  for (int i = 0; i < formatContext->nb_streams; i++) {
    if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      videoStreamIndex = i;
      break;
    }
  }
  if (videoStreamIndex == -1) {
    printf("No video stream found\n");
    return -1;
  }

  codec = avcodec_find_decoder(
      formatContext->streams[videoStreamIndex]->codecpar->codec_id);
  if (!codec) {
    printf("Unsupported codec\n");
    return -1;
  }

  codecContext = avcodec_alloc_context3(codec);
  if (!codecContext) {
    printf("Failed to allocate codec context\n");
    return -1;
  }

  if (avcodec_parameters_to_context(
          codecContext, formatContext->streams[videoStreamIndex]->codecpar) <
      0) {
    printf("Failed to copy codec parameters to context\n");
    return -1;
  }

  if (avcodec_open2(codecContext, codec, NULL) < 0) {
    printf("Failed to open codec\n");
    return -1;
  }

  AVFrame *frame = av_frame_alloc();

  SDL_Window *window = SDL_CreateWindow("Video Player", codecContext->width,
                                        codecContext->height, 0);
  if (!window) {
    printf("Failed to create window: %s\n", SDL_GetError());
    return -1;
  }

  SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
  if (!renderer) {
    printf("Failed to create renderer: %s\n", SDL_GetError());
    return -1;
  }

  // create a YUV texture
  SDL_Texture *texture = SDL_CreateTexture(
      renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING,
      codecContext->width, codecContext->height);
  if (!texture) {
    printf("Failed to create texture: %s\n", SDL_GetError());
    return -1;
  }

  struct SwsContext *swsContext = sws_getContext(
      codecContext->width, codecContext->height, codecContext->pix_fmt,
      codecContext->width, codecContext->height, AV_PIX_FMT_YUV420P,
      SWS_BILINEAR, NULL, NULL, NULL);
  if (!swsContext) {
    printf("Failed to get context\n");
    return -1;
  }

  AVPacket packet;
  int uvPlanePitch = codecContext->width / 2;
  size_t yPlaneSize = codecContext->width * codecContext->height;
  size_t uvPlaneSize = yPlaneSize / 4;
  yPlane = (Uint8 *)malloc(yPlaneSize);
  uPlane = (Uint8 *)malloc(uvPlaneSize);
  vPlane = (Uint8 *)malloc(uvPlaneSize);
  if (!yPlane || !uPlane || !vPlane) {
    printf("Failed to allocate memory for planes\n");
    return -1;
  }

  double frameDelay =
      av_q2d(formatContext->streams[videoStreamIndex]->avg_frame_rate);
  if (frameDelay > 0) {
    frameDelay = 1.0 / frameDelay * 1000.0;
  } else {
    frameDelay = 40.0;
  }
  printf("Video frame delay: %f\n", frameDelay);

  SDL_Event event;
  int quit = 0;

  while (av_read_frame(formatContext, &packet) >= 0 && !quit) {
    if (packet.stream_index == videoStreamIndex) {
      if (avcodec_send_packet(codecContext, &packet) < 0) {
        printf("Failed to send packet\n");
        av_packet_unref(&packet);
        continue;
      }
      if (avcodec_receive_frame(codecContext, frame) < 0) {
        av_packet_unref(&packet);
        continue;
      }

      AVFrame frameYUV;
      frameYUV.data[0] = yPlane;
      frameYUV.data[1] = uPlane;
      frameYUV.data[2] = vPlane;
      frameYUV.linesize[0] = codecContext->width;
      frameYUV.linesize[1] = uvPlanePitch;
      frameYUV.linesize[2] = uvPlanePitch;

      sws_scale(swsContext, (uint8_t const *const *)frame->data,
                frame->linesize, 0, codecContext->height, frameYUV.data,
                frameYUV.linesize);

      SDL_UpdateYUVTexture(texture, NULL, yPlane, codecContext->width, uPlane,
                           uvPlanePitch, vPlane, uvPlanePitch);

      SDL_RenderClear(renderer);
      SDL_RenderTexture(renderer, texture, NULL, NULL);
      SDL_RenderPresent(renderer);

      SDL_Delay((Uint32)frameDelay);

      while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
          quit = 1;
          break;
        }
      }
    }
    av_packet_unref(&packet);
  }

  while (!quit) {
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        quit = 1;
        break;
      }
    }
    SDL_Delay(100);
  }

  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  av_frame_free(&frame);
  free(yPlane);
  free(uPlane);
  free(vPlane);
  sws_freeContext(swsContext);
  avcodec_free_context(&codecContext);
  avformat_close_input(&formatContext);

  printf("Application ended successfully\n");

  return 0;
}