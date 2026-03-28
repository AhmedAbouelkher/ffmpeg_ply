#include <libavcodec/codec.h>
#include <libavutil/frame.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <stdint.h>
#include <stdio.h>

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame);

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: %s <input file>\n", argv[0]);
    return -1;
  }

  AVFormatContext *formatContext = NULL;
  AVCodecContext *codecContext = NULL;
  const AVCodec *codec = NULL;

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
  if (codec == NULL) {
    printf("Unsupported codec\n");
    return -1;
  }

  codecContext = avcodec_alloc_context3(codec);
  if (codecContext == NULL) {
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
  AVFrame *frameRGB = av_frame_alloc();
  if (frame == NULL || frameRGB == NULL) {
    printf("Failed to allocate frames\n");
    return -1;
  }

  if (av_image_alloc(frameRGB->data, frameRGB->linesize, codecContext->width,
                     codecContext->height, AV_PIX_FMT_RGB24, 1) < 0) {
    printf("Failed to allocate image\n");
    return -1;
  }

  struct SwsContext *swsContext = sws_getContext(
      codecContext->width, codecContext->height, codecContext->pix_fmt,
      codecContext->width, codecContext->height, AV_PIX_FMT_RGB24, SWS_BILINEAR,
      NULL, NULL, NULL);
  if (swsContext == NULL) {
    printf("Failed to get context\n");
    return -1;
  }

  AVPacket packet;

  int i = 0;
  while (av_read_frame(formatContext, &packet) >= 0) {
    if (packet.stream_index == videoStreamIndex) {
      if (avcodec_send_packet(codecContext, &packet) < 0) {
        printf("Failed to send packet\n");
        return -1;
      }
      if (avcodec_receive_frame(codecContext, frame) < 0) {
        printf("Failed to receive frame\n");
        return -1;
      }

      int ret = sws_scale(swsContext, (uint8_t const *const *)frame->data,
                          frame->linesize, 0, codecContext->height,
                          frameRGB->data, frameRGB->linesize);
      if (ret <= 0) {
        printf("Failed to scale frame: %d\n", ret);
        return -1;
      }

      if (i % 100 == 0) {
        SaveFrame(frameRGB, codecContext->width, codecContext->height, i);
      }
      i++;
    }
  }
  // free the packet
  av_packet_unref(&packet);

  av_frame_free(&frame);
  av_frame_free(&frameRGB);
  sws_freeContext(swsContext);
  avcodec_free_context(&codecContext);
  avformat_close_input(&formatContext);

  printf("Application ended successfully\n");

  return 0;
}

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
  FILE *pFile;
  char szFilename[32];
  int y;

  // Open file
  sprintf(szFilename, "frame%d.ppm", iFrame);
  pFile = fopen(szFilename, "wb");
  if (pFile == NULL)
    return;

  // Write header
  fprintf(pFile, "P6\n%d %d\n255\n", width, height);

  // Write pixel data
  for (y = 0; y < height; y++)
    fwrite(pFrame->data[0] + y * pFrame->linesize[0], 1, width * 3, pFile);

  // Close file
  fclose(pFile);
}