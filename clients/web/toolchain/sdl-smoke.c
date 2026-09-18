#include <SDL.h>

int main(void) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    SDL_Window *window = SDL_CreateWindow("Ultimatum WebAssembly Smoke Test",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 320, 200, 0);
    if (!window) return 2;
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
