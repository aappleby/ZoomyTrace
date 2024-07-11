#include <memory.h>
//#include <stdio.h>
//#include <unistd.h>
//#include <assert.h>
//#include <sys/time.h>
#include <SDL2/SDL.h>
//#include <time.h>
//#include <map>
//
//#include "glad/glad.h"
#include "imgui/imgui.h"
//#include "GLBase.h"
#include "gui.hpp"
#include "log.hpp"

#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <thread>
#include <optional>

#include "capture.hpp"
#include <poll.h>
#include "BitChunk.hpp"

void log(const char* format, ...);
void err(const char* format, ...);

//------------------------------------------------------------------------------

int main(int argc, char** argv) {
  log("ZoomyTrace start");

  BitBuf buf;
  size_t sample_count = 64 * 1024;
  buf.init(sample_count);
  gen_pattern(buf.bits, sample_count);
  buf.update();
  buf.dump();

  return 0;

  Capture* cap = new Capture();

  cap->start_thread();

  //----------------------------------------

#if 1
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

    while (!cap->cap_to_host.empty()) {
      auto res = cap->cap_to_host.get();
      log("<- %-16s 0x%08x 0x%016x %ld", capcmd_to_cstr(res.command), res.result, res.block, res.length);
    }

    gui.update(window);

    ImGui::Begin("Capture Status");

    //static int packet_size = 1;
    //ImGui::SliderInt("packet size", &packet_size, 1, 1024, "%d", ImGuiSliderFlags_Logarithmic);
    //double p = exp2(ceil(log2(packet_size)));
    //packet_size = (int)p;

    ImGui::Text("thread_running  %d", (bool)cap->thread_running);
    ImGui::Text("device_present  %d", (bool)cap->device_present);
    ImGui::Text("capture_running %d", (bool)cap->capture_running);
    ImGui::Text("libusb_ctx      %p", cap->ctx);
    ImGui::Text("libusb_hdev     %p", cap->hdev);
    ImGui::Text("epoll_fd        %d", cap->epoll_fd);
    ImGui::Text("hotplug_handle  %d", cap->hotplug_handle);
    ImGui::Text("capture_thread  %p", cap->capture_thread);

    ImGui::Text("bulk_requested  %d", (int)cap->bulk_requested);
    ImGui::Text("bulk_submitted  %d", (int)cap->bulk_submitted);
    ImGui::Text("bulk_pending    %d", (int)cap->bulk_pending);
    ImGui::Text("bulk_done       %d", (int)cap->bulk_done);

    /*
    std::queue<libusb_transfer*> control_pool;
    std::queue<libusb_transfer*> bulk_pool;
    */

    if (ImGui::TreeNode("Ring Buffer Settings")) {
      ImGui::Text("ring_buffer     %p", cap->ring_buffer);
      ImGui::Text("ring_size       %d", cap->ring_buffer_len);
      ImGui::Text("ring_chunk      %d", cap->chunk_size);

      ImGui::Text("ring_read       %d", (int)cap->ring_read );
      ImGui::Text("ring_ready      %d", (int)cap->ring_ready);
      ImGui::Text("ring_write      %d", (int)cap->ring_write);

      ImGui::TreePop();
    }

    if (!cap->hdev) {
      if (ImGui::Button("connect",   {100,25})) {
        cap->post_async({XCMD_CONNECT, 0, 0, 0});
      }
    }
    else {
      if (ImGui::Button("disconnect",   {100,25})) {
        cap->post_async({XCMD_DISCONNECT, 0, 0, 0});
      }

      if (ImGui::Button("get_fwid",  {100,25})) {
        cap->post_async({XCMD_GET_FWID, 0, 0, 0});
      }

      if (ImGui::Button("get_revid", {100,25})) {
        cap->post_async({XCMD_GET_REVID, 0, 0, 0});
      }

      ImGui::Button("start_cap", {100,25});
    }

    static int frame = 0;
    ImGui::Text("frame %06d", frame++);


    log_draw_imgui("log");


	  ImGui::End();

    ImGui::ShowDemoWindow();

    gui.render(window);
    SDL_GL_SwapWindow((SDL_Window*)window);
  }

#endif

  //----------------------------------------

  cap->stop_thread();
  delete cap;

  log("ZoomyTrace exit");
  return 0;
}
