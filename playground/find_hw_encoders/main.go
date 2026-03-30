package main

/*
#cgo LDFLAGS: -framework CoreMedia -framework CoreVideo -framework VideoToolbox
#include "find_hw_support.h"
*/
import "C"
import "fmt"

type CodecType uint32

const (
	CodecTypeH264       CodecType = C.H264_CODEC
	CodecTypeH265       CodecType = C.H265_CODEC
	CodecTypeH265_10Bit CodecType = C.H265_10BIT_CODEC
	CodecTypeAV1        CodecType = C.AV1_CODEC
)

func main() {
	codec := CodecTypeH264

	isAvailable := C.is_hardware_encoder_available(C.CodecType(codec))
	switch isAvailable {
	case 2:
		fmt.Println("Unsupported platform")
	case 1:
		fmt.Println("Hardware encoder is available")
	default:
		fmt.Println("Hardware encoder is not available")
	}
}
