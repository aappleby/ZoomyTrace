//#include "SDL2/include/SDL.h"
#include <SDL2/SDL.h>

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

void gen_pattern(void* buf, size_t sample_count) {

  printf("generating pattern\n");

  auto time_a = timestamp();

  uint8_t* bits = (uint8_t*)buf;

  uint32_t x = 1;
  for (size_t i = 0; i < sample_count; i++) {
    bits[i] = 0x00;

    //if (i & 1) bits[i] |= 1;
    //if (i & 2) bits[i] |= 2;
    //if (i & 4) bits[i] |= 4;
    //if (i & 8) bits[i] |= 8;
    //if (i & 16) bits[i] |= 16;
    //if (i & 32) bits[i] |= 32;
    //if (i & 64) bits[i] |= 64;
    //if (i & 128) bits[i] |= 128;

    size_t t = i;
    //t = t*((t>>9|t>>13)&25&t>>6);

    t *= 0x1234567;
    t ^= t >> 32;
    t *= 0x1234567;
    t ^= t >> 32;

    bits[i] = t;
    //bits[(i >> 5)] |= (__builtin_parity(t) << (i & 31));
  }

  printf("generating pattern done in %12.8f sec\n", timestamp() - time_a);
}

//------------------------------------------------------------------------------

int main(int argc, char* argv[]) {


  //size_t sample_count = 32ull * 1024ull * 1024ull * 1024ull;
  //size_t sample_count = 1024ull * 1024ull;
  //size_t bits_len = sample_count / 8;
  //uint8_t* bits = new uint8_t[bits_len];

  BitBlob blob;
  blob.channels = 8;
  blob.sample_count = 256ull * 1024ull * 1024ull;
  //blob.sample_count = 1024ull * 1024ull;
  blob.stride = 8;

  size_t max_index = blob.sample_count * blob.stride;
  blob.bits_len = (max_index + 7) / 8;
  blob.bits = new uint8_t[blob.bits_len];
  gen_pattern(blob.bits, blob.sample_count);

  BitMips mips[8];

  for (int i = 0; i < 8; i++) {
    mips[i].init();
    mips[i].update_mips(blob, 1);
  }

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
  double origin = blob.sample_count / 2.0;

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

    zoom   = view_control.view_smooth_snap.view_zoom();
    origin = view_control.view_smooth_snap.world_center().x + blob.sample_count / 2.0;

    origin += sin(new_now * 1.0) * 1.0 * exp2(-zoom);

    double pixels_per_sample = pow(2, zoom);
    double samples_per_pixel = 1.0 / pixels_per_sample;

    double bar_min = 0.0;
    double bar_max = WINDOW_WIDTH;

    double view_min = origin - ((WINDOW_WIDTH / 2.0) * samples_per_pixel);
    double view_max = origin + ((WINDOW_WIDTH / 2.0) * samples_per_pixel);

    double traces[8][WINDOW_WIDTH];

    //void render(void* bits, size_t sample_count, double bar_min, double bar_max, double view_min, double view_max, double* ch0_trace, int window_width);

    double time_a, time_b;

    /*
    // about 0.11 sec for full update of 2^28-bit blob
    time_a = timestamp();
    ch0_mips.update(blob, 0);
    time_b = timestamp();
    printf("update took %12.6f\n", time_b - time_a);
    */


    time_a = timestamp();
    for (int i = 0; i < 8; i++) {
      mips[i].render(blob, i, bar_min, bar_max, view_min, view_max, traces[i], WINDOW_WIDTH);
    }
    time_b = timestamp();
    printf("render took %12.6f\n", time_b - time_a);
    //fflush(stdout);

    //----------
    // Render

    time_a = timestamp();
    uint32_t* pixels;
    int pitch;
    SDL_LockTexture(texture, NULL, (void**) & pixels, &pitch);
    //printf("pitch %d\n", pitch);

    // sRGB conversion
    for (int i = 0; i < 8; i++) {
      for (int x = 0; x < WINDOW_WIDTH; x++) {
        double s = traces[i][x];
        s = (s < 0.0031308) ? 12.92 * s : (1.0 + 0.055) * pow(s, 1.0 / 2.4) - 0.055;
        int v = (int)floor(s * 255.0);
        traces[i][x] = v;
      }
    }

    for (int channel = 0; channel < 8; channel++) {
      for (int row = 0; row < 64; row++) {
        for (int x = 0; x < WINDOW_WIDTH; x++) {
          int y = 128 + channel * 96 + row;
          int v = (int)traces[channel][x];
          uint32_t color = (v << 24) | (v << 16) | (v << 8) | 0xFF;
          pixels[x + y * (pitch / sizeof(uint32_t))] = color;
        }
      }
    }


    SDL_UnlockTexture(texture);
    time_b = timestamp();
    printf("render took %12.6f\n", time_b - time_a);

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
