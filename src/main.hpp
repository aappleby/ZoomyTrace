#pragma once
#include "gui.hpp"
#include "ViewController.hpp"
#include "Blitter.hpp"
#include "TracePainter.hpp"
#include <SDL2/SDL.h>
#include <atomic>
#include <stdint.h>
#include "TraceMipper.hpp"

struct Capture;
struct SDL_Window;
struct BitBlob;
struct BitMips;

class Main {
public:

  void init();
  void update();
  void render();
  void exit();

  void update_imgui();

  Capture* cap;
  SDL_Window* window;
  Gui gui;
  double old_now;
  double new_now;
  double delta_time;
  ViewController vcon;
  Blitter blit;

  TracePainter trace_painter;
  TraceMipper  trace_mipper;



  int screen_w = 0;
  int screen_h = 0;
  int mouse_x = 0;
  int mouse_y = 0;
  int mouse_buttons = 0;
  bool quit = false;
  int frame = 0;
  //double zoom_per_tick = 0.0625;
  double zoom_per_tick = 0.25;
  //double zoom_per_tick = 1.0;
  const uint8_t* keyboard_state;
  int key_count;
  SDL_DisplayMode display_mode;
  SDL_Event events[256];

  // DMA ring buffer size seems to be limited to 8 megs (128 chunks)
  //int chunk_size  = 65536;
  //int chunk_count = 128;
  //int ring_size   = 65536 * 128;
  //int ring_mask   = (65536 * 128) - 1;

  dvec2 screen_size;

  TraceBuffer trace;
  MipBuffer   mips[8];

  std::atomic_int cursor_read  = 0;
  std::atomic_int cursor_ready = 0;
  std::atomic_int cursor_write = 0;

  double render_time;
};
