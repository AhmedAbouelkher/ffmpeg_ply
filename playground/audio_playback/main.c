#include <SDL2/SDL.h>
#include <math.h>

void audio_callback(void* userdata, Uint8* stream, int len) {
    double* phase = (double*)userdata;
    float* buffer = (float*)stream;
    int length = len / sizeof(float); // Number of samples

    for (int i = 0; i < length; i++) {
        // Generate sine wave (Frequency: 440Hz, Sample Rate: 44100)
        buffer[i] = (float)(0.2 * sin(*phase)); 
        *phase += 2.0 * M_PI * 440.0 / 44100.0;
    }
}

// void audio_callback(void *userdata, Uint8 *stream, int len) {
//   double *phase = (double *)userdata;
//   Sint16 *buffer = (Sint16 *)stream;
//   int length = len / sizeof(Sint16); // 2 bytes per sample

//   for (int i = 0; i < length; i++) {
//     // Amplitude 28000 (Max is 32767)
//     buffer[i] = (Sint16)(28000 * sin(*phase));

//     *phase += 2.0 * M_PI * 440.0 / 44100.0;

//     // Keep phase within 2*PI to prevent precision loss over time
//     if (*phase >= 2.0 * M_PI) {
//       *phase -= 2.0 * M_PI;
//     }
//   }
// }

int main() {
  if (SDL_Init(SDL_INIT_AUDIO) != 0) {
    printf("SDL_Init Error: %s\n", SDL_GetError());
    return -1;
  }

  double phase = 0.0;
  SDL_AudioSpec want;
  SDL_zero(want);

  want.freq = 44100;
  want.format = AUDIO_S16SYS;
  want.channels = 1;
  want.samples = 4096;
  want.callback = audio_callback;
  want.userdata = &phase;

  // SDL_OpenAudio returns 0 on success, -1 on error
  if (SDL_OpenAudio(&want, NULL) < 0) {
    fprintf(stderr, "Failed to open audio: %s\n", SDL_GetError());
    return 1;
  }

  // Start playback globally
  SDL_PauseAudio(0);

  SDL_Delay(2000); // Play for 2 seconds

  // Close the global device
  SDL_CloseAudio();

  SDL_Quit();
  return 0;
}