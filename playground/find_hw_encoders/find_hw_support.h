#ifndef FIND_SUPPORT_H
#define FIND_SUPPORT_H

#ifdef __APPLE__
#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>
#include <VideoToolbox/VideoToolbox.h>

int is_hardware_encoder_available_darwin(CMVideoCodecType codecType,
                                         CFStringRef level);
#endif

// create an enum for the codec types H264, H265, H265_10BIT, AV1
typedef enum {
  H264_CODEC,
  H265_CODEC,
  H265_10BIT_CODEC,
  AV1_CODEC,
} CodecType;

int is_hardware_encoder_available(CodecType codecType);

#endif
