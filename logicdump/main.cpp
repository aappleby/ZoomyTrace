//#include <memory.h>
//#include <stdio.h>
//#include <unistd.h>
//#include <assert.h>
//#include <sys/time.h>
//#include <SDL2/SDL.h>
//#include <time.h>
//#include <map>
//
//#include "glad/glad.h"
//#include "imgui/imgui.h"
//#include "GLBase.h"
//#include "gui.hpp"

#include <libusb-1.0/libusb.h>
#include <stdio.h>

#include "capture.hpp"

//------------------------------------------------------------------------------

/*
double timestamp() {
  static uint64_t _time_origin = 0;
  timespec ts;
  (void)timespec_get(&ts, TIME_UTC);
  uint64_t now = ts.tv_sec * 1000000000ull + ts.tv_nsec;
  if (!_time_origin) _time_origin = now;
  return double(now - _time_origin) / 1.0e9;
}
*/

//------------------------------------------------------------------------------

int main(int argc, char** argv) {
  printf("logicdump start\n");
  capture();
  printf("logicdump done\n");
  return 0;

#if 0
  int ret;

  int initial_screen_w, initial_screen_h;

  initial_screen_w = 1920;
  initial_screen_h = 1080;

  SDL_Window* window = create_gl_window("Logicdump", initial_screen_w, initial_screen_h);

  //----------------------------------------
  // Initialize ImGui and ImGui renderer

  Gui gui;
  gui.init();

  //----------------------------------------

  bool quit = false;
  while (!quit) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_QUIT:
            quit = true;
            break;
        case SDL_MOUSEWHEEL: {
          ImGuiIO& io = ImGui::GetIO();
          io.MouseWheelH += event.wheel.x;
          io.MouseWheel += event.wheel.y;
          break;
        }
      }
    }

    gui.update(window);

    ImGui::ShowDemoWindow();

    gui.render(window);
    SDL_GL_SwapWindow((SDL_Window*)window);
  }

  return 0;
#endif
}
