#include <SDL2/SDL.h>
#include <math.h>

#define AMPLITUDE 8000
#define SAMPLE_RATE 44100
#define FREQUENCY 440.0

// Global tracking of where we are in the sine wave
double sample_index = 0;

void audio_callback(void *userdata, Uint8 *stream, int len) {
  // SDL_MixAudio works on the 'stream' buffer.
  // We first clear it or fill a temporary buffer with our sine wave.
  Sint16 *buffer = (Sint16 *)stream;
  int length = len / 2; // len is in bytes, we need number of samples (16-bit)

  for (int i = 0; i < length; i++) {
    // Calculate the sine value
    double time = sample_index / SAMPLE_RATE;
    int16_t sample = (int16_t)(AMPLITUDE * sin(2.0 * M_PI * FREQUENCY * time));

    // SDL_MixAudio adds our sample to the existing stream
    // effectively "mixing" it with whatever else is playing.
    // For a simple demo, we can just assign it, or use SDL_MixAudioFormat:
    SDL_MixAudioFormat(stream + (i * 2), (Uint8 *)&sample, AUDIO_S16SYS, 2,
                       SDL_MIX_MAXVOLUME);

    sample_index++;
  }
}

int main() {
  SDL_Init(SDL_INIT_AUDIO);

  SDL_AudioSpec want;
  SDL_zero(want);
  want.freq = SAMPLE_RATE;
  want.format = AUDIO_S16SYS;
  want.channels = 1;
  want.samples = 2048;
  want.callback = audio_callback;

  SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
  SDL_PauseAudioDevice(dev, 0); // Start playing

  SDL_Delay(2000); // Play for 2 seconds

  SDL_CloseAudioDevice(dev);
  SDL_Quit();
  return 0;
}