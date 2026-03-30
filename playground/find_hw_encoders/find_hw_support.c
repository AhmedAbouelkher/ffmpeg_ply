#include "find_hw_support.h"

#ifdef __APPLE__
OSStatus encoder_properties(CMVideoCodecType codecType,
                            CFStringRef *encoderIDOut,
                            CFDictionaryRef *supportedPropertiesOut) {
  const void *keys[1] = {
      kVTVideoEncoderSpecification_RequireHardwareAcceleratedVideoEncoder};
  const void *values[1] = {kCFBooleanTrue};
  CFDictionaryRef specification =
      CFDictionaryCreate(NULL, keys, values, 1, &kCFTypeDictionaryKeyCallBacks,
                         &kCFTypeDictionaryValueCallBacks);

  OSStatus err = VTCopySupportedPropertyDictionaryForEncoder(
      1920, 1080, codecType, specification, encoderIDOut,
      supportedPropertiesOut);
  CFRelease(specification);
  return err;
}

int is_hardware_encoder_available_darwin(CMVideoCodecType codecType,
                                         CFStringRef level) {
  Boolean found = false;
  CFStringRef encoderIDOut;
  CFDictionaryRef supportedPropertiesOut;

  OSStatus err =
      encoder_properties(codecType, &encoderIDOut, &supportedPropertiesOut);

  if (err == noErr) {
    printf("[C] encoder_properties succeeded\n");
    if (level != NULL) {
      CFDictionaryRef profiles = CFDictionaryGetValue(
          supportedPropertiesOut, kVTCompressionPropertyKey_ProfileLevel);
      if (profiles != NULL) {
        printf("[C] Profiles dictionary found\n");
        CFArrayRef listOfValues =
            CFDictionaryGetValue(profiles, kVTPropertySupportedValueListKey);
        if (listOfValues != NULL) {
          printf("[C] Supported value list found, number of values: %ld\n",
                 CFArrayGetCount(listOfValues));
          found = CFArrayContainsValue(
              listOfValues, CFRangeMake(0, CFArrayGetCount(listOfValues)),
              level);
          if (found) {
            printf("[C] Level is present in the supported value list\n");
          } else {
            printf("[C] Level is NOT present in the supported value list\n");
          }
        } else {
          printf("[C] Supported value list for profile level not found\n");
        }
      } else {
        printf("[C] Profiles dictionary not found\n");
      }
    } else {
      found = true;
      printf("[C] No level supplied, assuming encoder is available\n");
    }

    CFRelease(encoderIDOut);
    CFRelease(supportedPropertiesOut);
  } else {
    printf("[C] encoder_properties failed with err = %d\n", (int)err);
  }
  return found;
}

// convert the codec type to the corresponding CMVideoCodecType
CMVideoCodecType codec_type_to_cm_video_codec_type(CodecType codecType) {
  switch (codecType) {
  case H264_CODEC:
    return kCMVideoCodecType_H264;
  case H265_CODEC:
    return kCMVideoCodecType_HEVC;
  case H265_10BIT_CODEC:
    return kCMVideoCodecType_HEVC;
  case AV1_CODEC:
    return kCMVideoCodecType_AV1;
  }
  return kCMVideoCodecType_H264;
}

#endif

int is_hardware_encoder_available(CodecType codecType) {
#ifdef __APPLE__
  CMVideoCodecType appleCodecType =
      codec_type_to_cm_video_codec_type(codecType);
  if (codecType == H265_10BIT_CODEC) {
    return is_hardware_encoder_available_darwin(
        appleCodecType, kVTProfileLevel_HEVC_Main10_AutoLevel);
  } else {
    return is_hardware_encoder_available_darwin(appleCodecType, NULL);
  }
#endif
  // TODO: add support for other platforms
  return -2;
}