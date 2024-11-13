#pragma once
#include <stdint.h>
#include "third_party/imgui/imgui.h"
#include <SDL2/SDL.h>

//------------------------------------------------------------------------------

struct Gui {

  uint32_t imgui_prog = 0;
  uint32_t imgui_tex = 0;
  uint32_t imgui_vao = 0;
  uint32_t imgui_vbo = 0;
  uint32_t imgui_ibo = 0;

  void init();
  void render_gl(SDL_Window* window);
};

//------------------------------------------------------------------------------

SDL_Window* create_gl_window(const char* name, int initial_screen_w, int initial_screen_h);
ImGuiKey keycode_to_imgui(SDL_Keycode keycode);
