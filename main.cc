#include <stdio.h>
#include "gameboy.hh"

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: gameboy [path/to/rom]");
    exit(1);
  }

  Gameboy gameboy(argv[1]);
  SDL_Event e;
  bool quit = false;
  while (!quit){
    while (SDL_PollEvent(&e)){
      if (e.type == SDL_QUIT){
        quit = true;
      }
      if (e.type == SDL_KEYDOWN){
        quit = true;
      }
    }
    gameboy.update();
  }
  return 1;
}
