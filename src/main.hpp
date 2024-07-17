#pragma once
#include "gui.hpp"
#include "ViewController.hpp"
#include "Blitter.hpp"
#include "TracePainter.hpp"
#include "RingBuffer.hpp"
#include <SDL2/SDL.h>

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
  int screen_w = 0;
  int screen_h = 0;
  int mouse_x = 0;
  int mouse_y = 0;
  int mouse_buttons = 0;
  bool quit = false;
  int frame = 0;
  //double zoom_per_tick = 0.0625;
  //double zoom_per_tick = 0.25;
  double zoom_per_tick = 1.0;
  const uint8_t* keyboard_state;
  int key_count;
  SDL_DisplayMode display_mode;
  SDL_Event events[256];

  BitBlob* blob;
  BitMips* mips[8];
  RingBuffer* ring;
};
