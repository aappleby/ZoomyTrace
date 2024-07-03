#include "SDL2/include/SDL.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <time.h>
#include <assert.h>
#include <math.h>

#ifdef _MSC_VER
#  include <intrin.h>
#  define __builtin_popcount __popcnt
#else
//#  include <builtin.h>
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

struct mybitvec {
  mybitvec() {}
  int64_t len;
  uint32_t bits  [sample_count / 32];
  uint32_t count1[sample_count / 256];
  uint32_t count2[sample_count / 65536];
  uint32_t count3[sample_count / 16777216];
};

mybitvec* line = nullptr;

//----------
// Crappy cubic sin(2*pi*x) approximation

double fsin(double x) {
  double y = abs(x - floor(x - 0.25) - 0.75) - 0.25;
  return y*(6.1922647442358311664092063140774168 - 35.3637069400432002923807340734237452*y*y);
}

void count_init() {
  line = new mybitvec();
  memset(line, 0, sizeof(mybitvec));

  printf("generating pattern\n");
  auto time_a = timestamp();

  for (int64_t i = 0; i < sample_count; i++) {
    double t = double(i) / double(sample_count);

    t *= 100.0;
    t = fsin(t) * t * 0.01;
    t = t * 0.5 + 0.5;
    t *= double(0xFFFFFFFF);

    int s = int(rng() <= t);
    //s = 1;

    if ((i % 17327797) < 4537841) {
      s = 0;
    }

    line->bits[(i >> 5)] |= (s << (i & 31));
    line->count1[i >>  8] += s;
    line->count2[i >> 16] += s;
    line->count3[i >> 24] += s;
  }

  line->len = sample_count;

  printf("%12.8f generating pattern done\n", timestamp() - time_a);

}

//----------

int step_0 = 0;
int step_1 = 0;
int step_2 = 0;
int step_3 = 0;

/*
step 0 12072
step 1 439606
step 2 63812
step 3 0
*/

int64_t count_bits(mybitvec* line, int64_t a, int64_t b) {
  int64_t head = a;
  int64_t tail = b - 1;
  uint32_t total = 0;

  uint32_t head_mask = 0xFFFFFFFF << (head & 0x0000001f);


  for (int64_t i = head; i <= tail; i++) {
    total += (line->bits[i >> 5] >> (i & 31)) & 1;
  }

  /*
  int64_t head_cell = head >> 5;
  int64_t tail_cell = tail >> 5;

  for (int64_t i = head_cell; i <= tail_cell; i++) {
    total += __builtin_popcount(line->bits[i]);
  }
  */

#if 0
  uint32_t head_mask = 0xFFFFFFFF << (head & 0x0000001f);
  uint32_t tail_mask = 0xFFFFFFFF >> (31 - (tail & 0x0000001f));

  if (head_cell == tail_cell) {
    return __builtin_popcount(line->bits[head_cell] & head_mask & tail_mask);
  }

  int64_t cursor = a;
  int64_t remain = b - a - 1;

  if (cursor & 0x1F && remain >= 0x1F) {
    total += __builtin_popcount(line->bits[head_cell] & head_mask);
    remain -= cursor & 0x1F;
    cursor = (cursor + 0x1F) & ~0x1F;
  }

  while (remain > 32) {
    /*
    if (((cursor & 0x00FFFFF) == 0) && remain >= 0x01000000) {
      total += line->count3[cursor >> 24];
      cursor += 0x01000000;
      remain -= 0x01000000;
      step_3++;
    }
    else if (((cursor & 0x0000FFFF) == 0) && remain >= 0x00010000) {
      total += line->count2[cursor >> 16];
      cursor += 0x00010000;
      remain -= 0x00010000;
      step_2++;
    }
    else if (((cursor & 0x000000FF) == 0) && remain >= 0x00000100) {
      total += line->count1[cursor >> 8];
      cursor += 0x00000100;
      remain -= 0x00000100;
      step_1++;
    }
    else
    */
    {
      total += __builtin_popcount(line->bits[cursor >> 5]);
      cursor += 0x00000020;
      remain -= 0x00000020;
      step_0++;
    }
  }

  if (remain) {
    total += __builtin_popcount(line->bits[cursor >> 5] & tail_mask);
  }
#endif

  return total;
}

int get_bit(mybitvec* line, int64_t i) {
  if (i < 0) return 0;
  if (i >= line->len) return 0;
  return (line->bits[i >> 5] >> (i & 31)) & 1;
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

  zoom = 6.000000;
  origin = 8427316.578125;

  while (!quit) {
    //puts("\033[2J\033[H");

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
        //double zoom_per_tick = 0.0625;
        double zoom_per_tick = 0.25;
        //double zoom_per_tick = 1.0;
        view_control.on_mouse_wheel(mouse_pos_screen, screen_size, double(event.wheel.y) * zoom_per_tick);
      }

      if (event.type == SDL_MOUSEMOTION) {
        if (event.motion.state & SDL_BUTTON_LMASK) {
          //printf("xrel %d\n", event.motion.xrel);
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

    zoom   = view_control.view_smooth_snap.view_zoom();
    origin = view_control.view_smooth_snap.world_center().x + sample_count / 2.0;

    origin += sin(new_now * 1.0) * 1.0 * exp2(-zoom);

    //printf("zoom %f\n", zoom);
    //printf("origin %f\n", origin);

    double pixels_per_sample = pow(2, zoom);
    double samples_per_pixel = 1.0 / pixels_per_sample;

    double bar_min = 0.0;
    double bar_max = WINDOW_WIDTH;

    double view_min = origin - ((WINDOW_WIDTH / 2.0) * samples_per_pixel);
    double view_max = origin + ((WINDOW_WIDTH / 2.0) * samples_per_pixel);

    double filtered[WINDOW_WIDTH];

    double time_a = timestamp();

    {
      double sample_min = remap(0, bar_min, bar_max, view_min, view_max);
      double sample_max = remap(1, bar_min, bar_max, view_min, view_max);
      printf("pixel width in sample space %f\n", sample_max - sample_min);
    }

    step_0 = 0;
    step_1 = 0;
    step_2 = 0;
    step_3 = 0;


    for (int x = 0; x < WINDOW_WIDTH; x++) {
      filtered[x] = 0;

      double pixel_center = x + 0.5;

      double sample_fmin = remap(pixel_center - 0.5, bar_min, bar_max, view_min, view_max);
      double sample_fmax = remap(pixel_center + 0.5, bar_min, bar_max, view_min, view_max);

      if (sample_fmin < 0)            sample_fmin = 0;
      if (sample_fmax > sample_count) sample_fmax = sample_count;

      if (sample_fmax < 0)             { filtered[x] = 0; continue; }
      if (sample_fmin >= sample_count) { filtered[x] = 0; continue; }

      // the 5 at the end there means 1/(2^5) subpixel precision, seems like a good compromise
      double granularity = exp2(ceil(log2(sample_fmax - sample_fmin)) - 8);
      sample_fmin = ceil (sample_fmin / granularity) * granularity;
      sample_fmax = floor(sample_fmax / granularity) * granularity;

      // Samples coords are 32.8 fixed point

      int64_t sample_imin = (int64_t)floor(sample_fmin * 256.0);
      int64_t sample_imax = (int64_t)floor(sample_fmax * 256.0);

      // Both endpoints are inside the same sample.
      if ((sample_imin >> 8) == (sample_imax >> 8)) {
        filtered[x] = get_bit(line, sample_imin >> 8);
        continue;
      }

      int64_t sample_ilen = sample_imax - sample_imin;


      int64_t total = 0;

      if (sample_imin & 0xFF) {
        double head_fract = 256 - (sample_imin & 0xFF);
        total += head_fract * get_bit(line, sample_imin >> 8);
        sample_imin = (sample_imin + 0xFF) & ~0xFF;
      }

      if (sample_imax & 0xFF) {
        double tail_fract = sample_imax & 0xFF;
        total += tail_fract * get_bit(line, sample_imax >> 8);
        sample_imax = sample_imax & ~0xFF;
      }

      // Samples coords are now 32-bit integers

      assert(((sample_imax - sample_imin) & 0xFF) == 0);
      sample_imin >>= 8;
      sample_imax >>= 8;

      // Both endpoints are inside the same 32-sample block, this is the last step.
      if ((sample_imin >> 5) == ((sample_imax - 1) >> 5)) {
        uint32_t mask_min = 0xFFFFFFFF << (sample_imin & 0x1f);
        uint32_t mask_max = 0xFFFFFFFF >> (31 - ((sample_imax - 1) & 0x1f));
        uint32_t mask = mask_min & mask_max;
        total += __builtin_popcount(line->bits[sample_imin >> 5] & mask) * 256;
        filtered[x] = double(total) / double(sample_ilen);
        continue;
      }

      if (sample_imin & 0x1F) {
        uint32_t mask_min = 0xFFFFFFFF << (sample_imin & 0x1f);
        total += __builtin_popcount(line->bits[sample_imin >> 5] & mask_min) * 256;
        sample_imin = (sample_imin + 0x1f) & ~0x1f;
      }

      if (sample_imax & 0x1F) {
        uint32_t mask_max = 0xFFFFFFFF >> (31 - ((sample_imax - 1) & 0x1f));
        total += __builtin_popcount(line->bits[sample_imax >> 5] & mask_max) * 256;
        sample_imax = (sample_imax) & ~0x1f;
      }

      // Endpoints are now at multpiles of 32 samples.

      if (sample_imin == sample_imax) {
        filtered[x] = double(total) / double(sample_ilen);
        continue;
      }

      // Roll sample_imin forward to the next chunk of 256 samples
      while ((sample_imin & 0xFF) && (sample_imin < sample_imax)) {
        step_0++;
        total += __builtin_popcount(line->bits[sample_imin >> 5]) * 256;
        sample_imin += 32;
      }

      // Roll sample_imax backwards to the end of the previous chunk of 256 samples.
      while ((sample_imax & 0xFF) && (sample_imax > sample_imin)) {
        step_0++;
        total += __builtin_popcount(line->bits[(sample_imax - 1) >> 5]) * 256;
        sample_imax -= 32;
      }

      // Endpoints are now at multpiles of 256 samples.

      if (sample_imin == sample_imax) {
        filtered[x] = double(total) / double(sample_ilen);
        continue;
      }

      while ((sample_imin & 0xFFFF) && (sample_imin < sample_imax)) {
        step_1++;
        total += line->count1[sample_imin >> 8] * 256;
        sample_imin += 256;
      }

      while ((sample_imax & 0xFFFF) && (sample_imin < sample_imax)) {
        step_1++;
        total += line->count1[(sample_imax - 1) >> 8] * 256;
        sample_imax -= 256;
      }

      // Endpoints are now at multpiles of 65536 samples.

      if (sample_imin == sample_imax) {
        filtered[x] = double(total) / double(sample_ilen);
        continue;
      }

      while ((sample_imin && 0xFFFFFF) && (sample_imin < sample_imax)) {
        step_2++;
        total += line->count2[sample_imin >> 16] * 256;
        sample_imin += 65536;
      }

      while ((sample_imax & 0xFFFFFF) && (sample_imin < sample_imax)) {
        step_2++;
        total += line->count2[(sample_imax - 1) >> 16] * 256;
        sample_imax -= 65536;
      }

      // Endpoints are now at multpiles of 16777216 samples.

      if (sample_imin == sample_imax) {
        filtered[x] = double(total) / double(sample_ilen);
        continue;
      }

      for (int64_t i = sample_imin; i < sample_imax; i += 16777216) {
        step_3++;
        total += line->count3[i >> 24] * 256;
      }

      /*
      for (int64_t i = sample_imin; i < sample_imax; i++) {
        total += get_bit(&line, i) * 256;
      }
      */


      /*
      double head_fract = 1 - (sample_min - floor(sample_min));
      double tail_fract = sample_max - floor(sample_max);

      int head_bit = get_bit(&line, sample_imin);
      int body_bit = count_bits(&line, sample_imin + 1, sample_imax);
      int tail_bit = get_bit(&line, sample_imax);

      filtered[x] = ((head_bit * head_fract) + body_bit + (tail_bit * tail_fract)) / (sample_max - sample_min);
      */

      filtered[x] = double(total) / double(sample_ilen);
    }

    double time_b = timestamp();
    printf("update took %12.6f\n", time_b - time_a);
    printf("step 0 %d\n", step_0);
    printf("step 1 %d\n", step_1);
    printf("step 2 %d\n", step_2);
    printf("step 3 %d\n", step_3);
    fflush(stdout);

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
