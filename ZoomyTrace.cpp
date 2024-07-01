#include "SDL2/include/SDL.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <time.h>
#include <assert.h>

#ifdef _MSC_VER
#  include <intrin.h>
#  define __builtin_popcount __popcnt
#else
#  include <builtin.h>
#endif

#include "ViewController.hpp"

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080

double timestamp() {
  static uint64_t _time_origin = 0;
  timespec ts;
  (void)timespec_get(&ts, TIME_UTC);
  uint64_t now = ts.tv_sec * 1000000000ull + ts.tv_nsec;
  if (!_time_origin) _time_origin = now;
  return double(now - _time_origin) / 1.0e9;
}

uint32_t rng() {
  static uint32_t a = 1;
  a *= 0x12375457;
  a ^= a << 13;
  a ^= a >> 17;
  a ^= a << 5;
  return a;
}

//------------------------------------------------------------------------------

//const int64_t sample_count = 4ll * 1024ll * 1024ll * 1024ll;
const int64_t sample_count = 16 * 1024 * 1024;

uint32_t line_bits[sample_count / 32];
//uint32_t line_d0[sample_count];
//uint32_t line_d1[sample_count / 256];
//uint32_t line_d2[sample_count / 65536];
//uint32_t line_d3[sample_count / 16777216];


//----------

void count_init() {
  //memset(line_d0, 0, sizeof(line_d0));
  memset(line_bits, 0, sizeof(line_bits));

  printf("generating pattern\n");

  for (int64_t i = 0; i < sample_count; i++) {
    double t = double(i) / double(sample_count);
    t *= 2.0 * 3.14159265358979;
    t *= 100.0;
    t = (1.0 - sin(t)) / 2.0;
    t *= double(0xFFFFFFFF);

    int s = int(rng() <= t);

    line_bits[(i >> 5)] |= (s << (i & 31));

    //line_d0[i >>  0] += s;
    //line_d1[i >>  8] += s;
    //line_d2[i >> 16] += s;
    //line_d3[i >> 24] += s;
  }

  printf("generating pattern done\n");

}

//----------

int64_t count_bits(uint32_t* bitvec, int64_t len, int64_t a, int64_t b) {
  if (a > b) { auto t = a; a = b; b = t; }

  if (b < 0) return 0;
  if (a >= len) return 0;

  if (a < 0) a = 0;
  if (b > len) b = len;

  int64_t head = a;
  int64_t tail = b - 1;
  uint32_t total = 0;

  int64_t head_cell = head >> 5;
  int64_t tail_cell = tail >> 5;

  uint32_t head_mask = 0xFFFFFFFF << (head & 0x0000001f);
  uint32_t tail_mask = 0xFFFFFFFF >> (31 - (tail & 0x0000001f));

  if (head_cell == tail_cell) {
    return __builtin_popcount(bitvec[head_cell] & head_mask & tail_mask);
  }

  total += __builtin_popcount(bitvec[head_cell] & head_mask);

  for (int64_t cell = head_cell + 1; cell < tail_cell; cell++) {
    total += __builtin_popcount(bitvec[cell]);
  }

  total += __builtin_popcount(bitvec[tail_cell] & tail_mask);

  return total;
}

//------------------------------------------------------------------------------

int64_t round_nearest(double x) { return (int64_t)floor(x + 0.5); }

double remap(double x, double a1, double a2, double b1, double b2) {
  x -= a1;
  x /= (a2 - a1);
  x *= (b2 - b1);
  x += b1;
  return x;
}

int main(int argc, char* argv[]) {

  /*
  {
    uint32_t somebits[4] = {
      0b11111111111111110000000000000000,
      0b00000000000000001111111111111111,
      0b11111111111111111111111111111111,
      0b00000000000000000000000011110000,
    };
    int len = sizeof(somebits) * 32;

    int result;
    result = count_bits(somebits, len, 17, 47);
    printf("count %d\n", result);
    return 0;
  }
  */

  count_init();

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

  {
    uint32_t* pixels;
    int pitch;
    SDL_LockTexture(texture, NULL, (void**)&pixels, &pitch);
    SDL_memset(pixels, 0x00, pitch * WINDOW_HEIGHT);
    SDL_UnlockTexture(texture);
  }

  double zoom = 0.0;
  double origin = sample_count / 2.0;

  printf("derp\n");

  ViewController view_control;

  int screen_w = 0, screen_h = 0;
  SDL_GL_GetDrawableSize((SDL_Window*)window, &screen_w, &screen_h);
  dvec2 screen_size = { (double)screen_w, (double)screen_h };

  view_control.init(screen_size);

  //----------------------------------------
  // Main loop

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
        //printf("wheel\n");
        //double zoom_per_tick = 0.25;
        double zoom_per_tick = 1.0;
        view_control.on_mouse_wheel(mouse_pos_screen, screen_size, double(event.wheel.y) * zoom_per_tick);
      }

      if (event.type == SDL_MOUSEMOTION) {
        if (event.motion.state & SDL_BUTTON_LMASK) {
          printf("xrel %d\n", event.motion.xrel);
          //printf("mouse x %d\n", event.motion.x);
          //printf("yrel %d\n", event.motion.yrel);
          dvec2 rel = { (double)event.motion.xrel, (double)event.motion.yrel };
          view_control.pan(rel);
        }
      }
    }

    view_control.update(delta);

    //printf("View zoom %f\n", view_control.view_smooth.view_zoom());
    //printf("View x %f\n", view_control.view_smooth.world_center().x);

    //----------
    // Update wave tex

    uint32_t* pixels;
    int pitch;
    SDL_LockTexture(texture, NULL, (void**) & pixels, &pitch);

    //zoom = -5.0 + 10.0 * sin(new_now * 0.5);
    zoom = view_control.view_smooth_snap.view_zoom();
    //origin = sample_count / 2.0 + sin(new_now * 1.1) * pow(2.0, -zoom) * 100.0;
    origin = view_control.view_smooth_snap.world_center().x + sample_count / 2.0;

    double pixels_per_sample = pow(2, zoom);
    double samples_per_pixel = 1.0 / pixels_per_sample;

    double bar_min = 0.0;
    double bar_max = WINDOW_WIDTH;

    double view_min = origin - ((WINDOW_WIDTH / 2.0) * samples_per_pixel);
    double view_max = origin + ((WINDOW_WIDTH / 2.0) * samples_per_pixel);

    double filtered[WINDOW_WIDTH];

    double time_a = timestamp();

    for (int x = 0; x < WINDOW_WIDTH; x++) {
      double pixel_center = x + 0.5;

      double sample_min = remap(pixel_center - 0.5, bar_min, bar_max, view_min, view_max);
      double sample_max = remap(pixel_center + 0.5, bar_min, bar_max, view_min, view_max);

      int64_t sample_imin = (int64_t)floor(sample_min);
      int64_t sample_imax = (int64_t)floor(sample_max) + 1;

      int64_t len = sample_imax - sample_imin;
      int64_t count = count_bits(line_bits, sample_count, sample_imin, sample_imax);

      filtered[x] = double(count) / double(len);



#if 0
      if (sample_min < 0)            sample_min = 0;
      if (sample_max > sample_count) sample_max = sample_count;

      if (sample_max < 0) { filtered[x] = 0; continue; }
      if (sample_min >= sample_count) { filtered[x] = 0; continue; }

      // |--0--|--1--|--2--|--3--|
      // 0-----1-----2-----3-----4
      //          1..|-----|..3

      int64_t sample_imin = int64_t(ceil(sample_min) - 1);
      int64_t sample_imax = int64_t(floor(sample_max));

      double sample_min_fract = sample_imin + 1.0 - sample_min;
      double sample_max_fract = sample_max - sample_imax;
      
      if (floor(sample_min) == floor(sample_max)) {
        if (zoom <= -8) {
          filtered[x] = line_d1[sample_imin >> 8] * (1.0 / 256.0);
        }
        else {
          filtered[x] = line_d0[sample_imin];
        }
        continue;
      }

      {

        double accum_a = 0;
        double accum_b = 0;

        int64_t sample_cursor = sample_imin;

        while (sample_cursor <= sample_imax) {
          if (sample_cursor < 0) {
            sample_cursor++;
            continue;
          }
          if (sample_cursor >= sample_count) {
            sample_cursor++;
            continue;
          }

          double weight = 1.0;
          if (sample_cursor == sample_imin) weight = sample_min_fract;
          if (sample_cursor == sample_imax) weight = sample_max_fract;

          if (zoom <= -8) {
            accum_a += line_d1[sample_cursor >> 8] * weight * (1.0 / 256.0);
          }
          else {
            accum_a += line_d0[sample_cursor] * weight;
          }
          accum_b += weight;
          sample_cursor++;
        }

        filtered[x] = (accum_a / (sample_max - sample_min));
      }

      double divider_lerp = remap(zoom, 2.0, 4.0, 0.0, 1.0);
      if (divider_lerp < 0) divider_lerp = 0;
      if (divider_lerp > 1) divider_lerp = 1;

      if (ceil(sample_min) == floor(sample_max)) {
        double with_divider = filtered[x] * 0.6 + 0.1;
        filtered[x] = (filtered[x] * (1.0 - divider_lerp)) + (with_divider * divider_lerp);
      }
#endif
    }

    double time_b = timestamp();
    printf("update took %f\n", time_b - time_a);

    //----------
    // Render

    for (int y = 128 - 8; y < 128-4; y++) {
      for (int x = 0; x < WINDOW_WIDTH; x++) {
        pixels[x + y * (pitch / sizeof(uint32_t))] = 0xFFFFFFFF;
      }
    }
    for (int y = 128; y < 128+64; y++) {
      for (int x = 0; x < WINDOW_WIDTH; x++) {
        double s = filtered[x];
        //s = sqrt(s);
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