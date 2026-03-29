#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("SDL_Init Error: %s", SDL_GetError());
    return 1;
  }

  SDL_Window *window = SDL_CreateWindow("SDL3 on macOS", 640, 480, 0);
  if (!window) {
    SDL_Log("SDL_CreateWindow Error: %s", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  // Keep the window open, in this case SDL_Delay(5000); statement won't work.
  bool running = true;
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
      if (e.type == SDL_EVENT_QUIT) {
        printf("Quitting...");
        running = false;
        break;
      }
    }
  }

  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}