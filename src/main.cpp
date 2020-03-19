#include <iostream>
#include <stdint.h>
#include <SDL.h>
#include <thread>

#include "Chip8.h"

const int SCREEN_WIDTH = 1024;
const int SCREEN_HEIGHT = 512;

bool isRunning = true;
//const bool step = true;

uint8_t keymap[16] = {
    SDLK_x,
    SDLK_1,
    SDLK_2,
    SDLK_3,
    SDLK_q,
    SDLK_w,
    SDLK_e,
    SDLK_a,
    SDLK_s,
    SDLK_d,
    SDLK_z,
    SDLK_c,
    SDLK_4,
    SDLK_r,
    SDLK_f,
    SDLK_v,
};



int main(int argc, char* args[]) 
{
    SDL_Window *window = nullptr;

    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
        std::cerr << "Could not initialize SDL! SDL_ERROR: " << SDL_GetError() << std::endl;
        exit(1);
    } else {
        window = SDL_CreateWindow("Chip8", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
        if (window == nullptr)
        {
            std::cerr << "Could not create window! SDL_ERROR: " << SDL_GetError() << std::endl;
            exit(2);
        }
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

    SDL_Texture *sdlTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 64, 32);

    uint32_t pixels[2048];
    Chip8 chip8;

    chip8.LoadRom("../roms/PONG");

    while (isRunning)
    {
        SDL_Event e;

        // Poll Keyboard
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT){
                isRunning = false;
            }

            if (e.type == SDL_KEYDOWN)
            {
                for (int i = 0; i < 16; i++)
                {
                    if (e.key.keysym.sym == keymap[i])
                        chip8.key[i] = 1;
                }
            }

            if (e.type == SDL_KEYUP)
            {
                for (int i = 0; i < 16; i++)
                {
                    if (e.key.keysym.sym == keymap[i])
                        chip8.key[i] = 0;
                }
            }
        }

        // Run a clock cycle
        chip8.Update();

        // If previous clock cycle set draw flag, draw to the screen
        if (chip8.drawFlag)
        {
            // Store pixels in temporary buffer
            for (int i = 0; i < 2048; ++i) {
                uint8_t pixel = chip8.display[i];
                pixels[i] = (0x00FFFFFF * pixel) | 0xFF000000; // Makes 1 = 0xFFFFFFFF and 0 = 0xFF000000
            }

            // Update SDL texture
            SDL_UpdateTexture(sdlTexture, nullptr, pixels, 64 * sizeof(Uint32));
            // Clear screen and render
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, sdlTexture, nullptr, nullptr);
            SDL_RenderPresent(renderer);

            chip8.drawFlag = false;
        }

        std::this_thread::sleep_for(std::chrono::microseconds(1200));
    }

    SDL_DestroyTexture(sdlTexture);
    sdlTexture = nullptr;

    SDL_DestroyRenderer(renderer);
    renderer = nullptr;

    SDL_DestroyWindow(window);
    window = nullptr;

    SDL_Quit();
}
