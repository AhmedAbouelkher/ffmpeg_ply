# ffmpeg_ply

`ffmpeg_ply` is a simple implementation of a video player (similar to `ffplay`) written in C. It leverages **FFmpeg** for media decoding and **SDL2** for rendering video and audio.

> [!IMPORTANT]
> This project is still **under construction** 🏗️.

## Resources I am using

- [How to Write a Video Player in Less Than 1000 Lines](http://dranger.com/ffmpeg/): The primary guide for this implementation.
- [leandromoreira/ffmpeg-libav-tutorial](https://github.com/leandromoreira/ffmpeg-libav-tutorial): FFmpeg libav tutorial for learning media fundamentals.
- [leandromoreira/digital_video_introduction](https://github.com/leandromoreira/digital_video_introduction): Introduction to video technology and codecs.
- [rambod-rahmani/ffmpeg-video-player](https://github.com/rambod-rahmani/ffmpeg-video-player): Reference implementation.
- [ffplay.c](https://github.com/FFmpeg/FFmpeg/blob/master/fftools/ffplay.c): The official FFmpeg player source.

## How to Build

### Prerequisites

Ensure you have the following installed:

- `gcc`
- `FFmpeg` libraries (`libavformat`, `libavcodec`, `libavutil`, `libswscale`, `libswresample`)
- `SDL2`
- `pkg-config`

### Compilation

Use the provided `Makefile` to build the project:

```bash
make build
```

To clean the build artifacts:

```bash
make clean
```

## Usage

Run the player by passing a video file as an argument:

```bash
./player <input_file>
```

## Playground

The `playground/` directory contains various experimental programs used to test specific features of SDL2 and FFmpeg in isolation, such as:

- Audio playback and mixing.
- Simple SDL2 rendering.
- Synchronized audio/video playback experiments.

Each subdirectory in `playground/` typically contains its own standalone implementation for learning purposes.
