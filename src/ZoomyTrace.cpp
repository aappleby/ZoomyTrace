//#include "SDL2/include/SDL.h"
#include <SDL2/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <time.h>
#include <assert.h>
#include <math.h>

#include "log.hpp"
#include "Bits.hpp"

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

void gen_pattern(TraceBuffer& trace) {
  assert(trace.channels == 8);
  assert(trace.stride == 8);

  uint8_t* bits = (uint8_t*)trace.blob;

  for (size_t i = 0; i < trace.samples; i++) {

    //bits[i] = i;

    size_t t = i + 0x993781;
    t = t*((t>>9|t>>13)&25&t>>6);

    bits[i] = t;

    //size_t t = i;
    //t *= 0x23456789;
    //t ^= (t >> 45);
    //t *= 0x23456789;
    //t ^= (t >> 45);
    //bits[i] = t;
  }

}

//------------------------------------------------------------------------------

int main(int argc, char* argv[]) {

  double time_a, time_b;

  TraceBuffer trace;
  trace.samples  = 65536ull;
  trace.channels = 8;
  trace.stride   = 8;
  trace.ssbo_len = (trace.samples * trace.stride + 7) / 8;
  trace.ssbo     = -1;
  trace.blob     = new uint8_t[trace.ssbo_len];

  printf("generating pattern\n");
  time_a = timestamp();
  gen_pattern(trace);
  time_b = timestamp();
  printf("generating pattern done in %12.8f sec\n", time_b - time_a);


  time_a = timestamp();
  MipBuffer mips[8];
  for (int i = 0; i < 8; i++) {
    mips[i].samples = trace.samples;

    size_t len = (trace.samples + 127) / 128;
    mips[i].mip1 = new uint8_t[len];

    len = (len + 127) / 128;
    mips[i].mip2 = new uint8_t[len];

    len = (len + 127) / 128;
    mips[i].mip3 = new uint8_t[len];

    len = (len + 127) / 128;
    mips[i].mip4 = new uint8_t[len];

    update_mips(trace, i, 0, trace.samples, mips[i]);
  }
  time_b = timestamp();
  printf("generating mips took %f\n", time_b - time_a);

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

  //double old_now = timestamp();
  double new_now = timestamp();

  dvec2 zoom = {0,0};
  double origin = trace.samples / 2.0;

  ViewController view_control;

  int screen_w = 0, screen_h = 0;
  SDL_GL_GetDrawableSize((SDL_Window*)window, &screen_w, &screen_h);
  dvec2 screen_size = { (double)screen_w, (double)screen_h };

  view_control.init(screen_size);

  //----------------------------------------
  // Main loop

  zoom = {6.000000, 6.000000};
  origin = 8427316.578125;

  while (!quit) {

    //----------
    // Bookkeeping

    SDL_DisplayMode display_mode;
    SDL_GetCurrentDisplayMode(0, &display_mode);

    //old_now = new_now;
    new_now = timestamp();
    // Hax, force frame delta to be exactly the refresh interval
    //double dt = new_now - old_now;
    double dt = 1.0 / double(display_mode.refresh_rate);

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
        view_control.zoom(mouse_pos_screen, screen_size, double(event.wheel.y) * zoom_per_tick, 0);
      }

      if (event.type == SDL_MOUSEMOTION) {
        if (event.motion.state & SDL_BUTTON_LMASK) {
          dvec2 rel = { (double)event.motion.xrel, (double)event.motion.yrel };
          view_control.pan(rel);
        }
      }

      if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_ESCAPE) {
          if (event.key.keysym.mod & KMOD_LSHIFT) {
            quit = true;
          }
        }
      }
    }

    view_control.update(dt);

    //----------
    // Update wave tex

    zoom   = view_control.view_smooth_snap._zoom;
    origin = view_control.view_smooth_snap._center.x + trace.samples / 2.0;

    origin += sin(new_now * 1.0) * 1.0 * exp2(-zoom.x);

    double pixels_per_sample = pow(2, zoom.x);
    double samples_per_pixel = 1.0 / pixels_per_sample;

    double bar_min = 0.0;
    double bar_max = WINDOW_WIDTH;

    double view_min = origin - ((WINDOW_WIDTH / 2.0) * samples_per_pixel);
    double view_max = origin + ((WINDOW_WIDTH / 2.0) * samples_per_pixel);

    double traces[8][WINDOW_WIDTH];

    time_a = timestamp();
    for (int i = 0; i < 8; i++) {
      render(trace, mips[i], i, bar_min, bar_max, view_min, view_max, traces[i], WINDOW_WIDTH);
    }
    time_b = timestamp();
    printf("render trace took %12.6f\n", time_b - time_a);

    //----------
    // Render

    time_a = timestamp();
    uint32_t* pixels;
    int pitch;
    SDL_LockTexture(texture, NULL, (void**) & pixels, &pitch);

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
    printf("render view took %12.6f\n", time_b - time_a);

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
