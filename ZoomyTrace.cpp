#include "SDL2/include/SDL.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <time.h>
#include <assert.h>
#include <math.h>

#include "log.hpp"
#include "BitChunk.hpp"

#ifdef _MSC_VER
#  include <intrin.h>
#  define __builtin_popcount __popcnt
#else
//#  include <builtin.h>
#endif

#include "ViewController.hpp"

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080

//------------------------------------------------------------------------------

int main(int argc, char* argv[]) {

  BitBuf buf;
  size_t sample_count = 4ull * 1024ull * 1024ull * 1024ull;
  buf.init(sample_count);
  gen_pattern(buf.bits, sample_count);
  buf.update();

  //----------

  SDL_Window* window = NULL;
  SDL_Renderer* renderer = NULL;
  SDL_Texture* texture = NULL;
  SDL_Event event;
  int quit = 0;

  SDL_Init(SDL_INIT_VIDEO);
  window = SDL_CreateWindow("SDL2 Software Rendering", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE | SDL_RENDERER_PRESENTVSYNC);
  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, WINDOW_WIDTH, WINDOW_HEIGHT);

  SDL_RenderSetVSync(renderer, 1);

  double old_now = timestamp();
  double new_now = timestamp();

  double zoom = 0.0;
  double origin = sample_count / 2.0;

  ViewController view_control;

  int screen_w = 0, screen_h = 0;
  SDL_GL_GetDrawableSize((SDL_Window*)window, &screen_w, &screen_h);
  dvec2 screen_size = { (double)screen_w, (double)screen_h };

  view_control.init(screen_size);

  //----------------------------------------
  // Main loop

  zoom = 6.000000;
  origin = 8427316.578125;

  while (!quit) {

    //----------
    // Bookkeeping

    old_now = new_now;
    new_now = timestamp();
    double delta = new_now - old_now;

    int screen_w = 0, screen_h = 0;
    SDL_GL_GetDrawableSize((SDL_Window*)window, &screen_w, &screen_h);
    dvec2 screen_size = { (double)screen_w, (double)screen_h };

    int mouse_x = 0, mouse_y = 0;
    SDL_GetMouseState(&mouse_x, &mouse_y);
    dvec2 mouse_pos_screen = { (double)mouse_x, (double)mouse_y };

    //----------
    // UI events

    while (SDL_PollEvent(&event) != 0) {
      if (event.type == SDL_QUIT) quit = 1;

      if (event.type == SDL_MOUSEWHEEL) {
        //double zoom_per_tick = 0.0625;
        double zoom_per_tick = 0.25;
        //double zoom_per_tick = 1.0;
        view_control.on_mouse_wheel(mouse_pos_screen, screen_size, double(event.wheel.y) * zoom_per_tick);
      }

      if (event.type == SDL_MOUSEMOTION) {
        if (event.motion.state & SDL_BUTTON_LMASK) {
          dvec2 rel = { (double)event.motion.xrel, (double)event.motion.yrel };
          view_control.pan(rel);
        }
      }
    }

    view_control.update(delta);

    //----------
    // Update wave tex

    uint32_t* pixels;
    int pitch;
    SDL_LockTexture(texture, NULL, (void**) & pixels, &pitch);

    zoom   = view_control.view_smooth_snap.view_zoom();
    origin = view_control.view_smooth_snap.world_center().x + sample_count / 2.0;

    origin += sin(new_now * 1.0) * 1.0 * exp2(-zoom);

    double pixels_per_sample = pow(2, zoom);
    double samples_per_pixel = 1.0 / pixels_per_sample;

    double bar_min = 0.0;
    double bar_max = WINDOW_WIDTH;

    double view_min = origin - ((WINDOW_WIDTH / 2.0) * samples_per_pixel);
    double view_max = origin + ((WINDOW_WIDTH / 2.0) * samples_per_pixel);

    double filtered[WINDOW_WIDTH];

    //void update_table(void* bits, size_t sample_count, double bar_min, double bar_max, double view_min, double view_max, double* filtered, int window_width);
    double time_a = timestamp();
    buf.update_table(bar_min, bar_max, view_min, view_max, filtered, WINDOW_WIDTH);
    double time_b = timestamp();
    printf("update took %12.6f\n", time_b - time_a);
    //fflush(stdout);

    //----------
    // Render

    for (int y = 128 - 8; y < 128-4; y++) {
      for (int x = 0; x < WINDOW_WIDTH; x++) {
        pixels[x + y * (pitch / sizeof(uint32_t))] = 0x7F7F7FFF;
      }
    }
    for (int y = 128; y < 128+64; y++) {
      for (int x = 0; x < WINDOW_WIDTH; x++) {
        double s = filtered[x];
        s = (s < 0.0031308) ? 12.92 * s : (1.0 + 0.055) * pow(s, 1.0 / 2.4) - 0.055;
        int v = (int)floor(s * 255.0);
        uint32_t color = (v << 24) | (v << 16) | (v << 8) | 0xFF;
        pixels[x + y * (pitch / sizeof(uint32_t))] = color;
      }
    }

    for (int y = 128+64; y < 128+64+4; y++) {
      for (int x = 0; x < WINDOW_WIDTH; x++) {
        pixels[x + y * (pitch / sizeof(uint32_t))] = 0xFFFFFFFF;
      }
    }

    SDL_UnlockTexture(texture);

    //----------
    // Swap

    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
  }

  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
