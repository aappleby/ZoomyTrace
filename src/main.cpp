#include "main.hpp"

#include "BitChunk.hpp"
#include "capture.hpp"
#include "GLBase.h"
#include "log.hpp"

#include "symlinks/glad/glad.h"
#include "symlinks/imgui/imgui.h"
#include <SDL2/SDL.h>

void log(const char* format, ...);
void err(const char* format, ...);

extern std::string log_buf;

//------------------------------------------------------------------------------

void Main::init() {
  log("ZoomyTrace init");

  cap = new Capture();
  cap->start_thread();

  int initial_screen_w, initial_screen_h;
  initial_screen_w = 1920;
  initial_screen_h = 1080;
  window = create_gl_window("Logicdump", initial_screen_w, initial_screen_h);

  blit.init();

  vcon.init({initial_screen_w, initial_screen_h});

  //----------------------------------------
  // Initialize ImGui and ImGui renderer

  gui.init();

  test_tex = create_texture_u32(1920, 1, nullptr, false);

  old_now = timestamp();
  new_now = timestamp();
  frame = -1;
}

//------------------------------------------------------------------------------

void Main::update() {

  SDL_GL_GetDrawableSize((SDL_Window*)window, &screen_w, &screen_h);
  mouse_buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
  keyboard_state = SDL_GetKeyboardState(&key_count);
  SDL_GetCurrentDisplayMode(0, &display_mode);

  // Hax, force frame delta to be exactly the refresh interval
  frame++;
  old_now = new_now;
  new_now = timestamp();
  delta_time = 1.0 / float(display_mode.refresh_rate);

  //----------------------------------------
  // Send input state to imgui

  ImGuiIO& imgui_io = ImGui::GetIO();

  imgui_io.DisplaySize.x = float(screen_w);
  imgui_io.DisplaySize.y = float(screen_h);
  imgui_io.DeltaTime = delta_time;

  //----------------------------------------
  // Feed SDL events to ImGui

  SDL_PumpEvents();
  int nevents = SDL_PeepEvents(events, 256, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);
  //if (nevents) printf("nevents = %d\n", nevents);
  assert(nevents <= 256);

  for (int i = 0; i < nevents; i++) {
    auto& event = events[i];
    if (event.type == SDL_MOUSEWHEEL)      imgui_io.AddMouseWheelEvent(event.wheel.x, event.wheel.y);
    if (event.type == SDL_MOUSEMOTION)     imgui_io.AddMousePosEvent(event.motion.x, event.motion.y);

    if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
      int mouse_button = 0;
      if (event.button.button == SDL_BUTTON_LEFT)   { mouse_button = 0; }
      if (event.button.button == SDL_BUTTON_RIGHT)  { mouse_button = 1; }
      if (event.button.button == SDL_BUTTON_MIDDLE) { mouse_button = 2; }
      if (event.button.button == SDL_BUTTON_X1)     { mouse_button = 3; }
      if (event.button.button == SDL_BUTTON_X2)     { mouse_button = 4; }
      imgui_io.AddMouseButtonEvent(mouse_button, event.type == SDL_MOUSEBUTTONDOWN);
    }

    if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
      auto mods = (SDL_Keymod)event.key.keysym.mod;
      imgui_io.AddKeyEvent(ImGuiMod_Ctrl,  (mods & KMOD_CTRL) != 0);
      imgui_io.AddKeyEvent(ImGuiMod_Shift, (mods & KMOD_SHIFT) != 0);
      imgui_io.AddKeyEvent(ImGuiMod_Alt,   (mods & KMOD_ALT) != 0);
      imgui_io.AddKeyEvent(ImGuiMod_Super, (mods & KMOD_GUI) != 0);
      imgui_io.AddKeyEvent(keycode_to_imgui(event.key.keysym.sym), (event.type == SDL_KEYDOWN));
    }

    if (event.type == SDL_TEXTINPUT) {
      imgui_io.AddInputCharactersUTF8(event.text.text);
    }
  }

  //----------------------------------------
  // Feed SDL events to ZoomyTrace

  for (int i = 0; i < nevents; i++) {
    auto& event = events[i];
    if (event.type == SDL_QUIT) {
      quit = true;
    }

    if (event.type == SDL_KEYDOWN && !imgui_io.WantCaptureKeyboard && !imgui_io.WantTextInput) {
      if ((event.key.keysym.sym == SDLK_ESCAPE) && (event.key.keysym.mod & KMOD_LSHIFT)) {
        quit = true;
      }
    }

    if (event.type == SDL_MOUSEWHEEL && !imgui_io.WantCaptureMouse) {
      dvec2 screen_size = { (double)screen_w, (double)screen_h };
      dvec2 mouse_pos_screen = { (double)mouse_x, (double)mouse_y };
      vcon.on_mouse_wheel(mouse_pos_screen, screen_size, double(event.wheel.y) * zoom_per_tick);
    }

    if (event.type == SDL_MOUSEMOTION && !imgui_io.WantCaptureMouse) {
      if (event.motion.state & SDL_BUTTON_LMASK) {
        dvec2 rel = { (double)event.motion.xrel, (double)event.motion.yrel };
        vcon.pan(rel);
      }
    }
  }

  //----------------------------------------

  /*
  while (!cap->cap_to_host.empty()) {
    auto res = cap->cap_to_host.get();
    log("<- %-16s 0x%08x 0x%016x %ld", capcmd_to_cstr(res.command), res.result, res.block, res.length);
  }
  */

  double delta = new_now - old_now;
  vcon.update(delta);

  update_imgui();

  uint32_t pixels[1920];
  for (int i = 0; i < 1920; i++) {
    uint8_t t = 0;
    if (cap->ring_buffer) {
      t = cap->ring_buffer[i];
    }
    else {
      t = rng() & 0xFF;
    }
    pixels[i] = 0xFF000000 | (t << 0) | (t << 8) | (t << 16);
  }

  update_texture_u32(test_tex, 1920, 1, pixels);
}

//------------------------------------------------------------------------------

void Main::update_imgui() {
  ImGui::NewFrame();

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

    if (ImGui::Button("start_cap", {100,25})) {
      cap->post_async({XCMD_START_CAP, 256, 0, 0});
    }

    if (ImGui::Button("stop_cap", {100,25})) {
      cap->post_async({XCMD_STOP_CAP, 0, 0, 0});
    }
  }

  ImGui::Text("frame %06d", frame);

  {
      ImGui::Begin("log");
      ImGui::BeginChild("scrolling", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
      ImGui::TextUnformatted(log_buf.data(), log_buf.data() + log_buf.size());
      ImGui::PopStyleVar();

      if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
          ImGui::SetScrollHereY(1.0f);
      ImGui::EndChild();
      ImGui::End();
  }

  ImGui::End();
  ImGui::ShowDemoWindow();
  ImGui::Render();
}

//------------------------------------------------------------------------------

void Main::render() {
  glViewport(0, 0, screen_w, screen_h);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  dvec2 screen_size = { (double)screen_w, (double)screen_h };
  blit.blit(vcon.view_smooth_snap, screen_size, test_tex, 0, 100, 1920, 64);

  gui.render_gl(window);

  SDL_GL_SwapWindow((SDL_Window*)window);
}

//------------------------------------------------------------------------------

void Main::exit() {
  cap->stop_thread();
  delete cap;
  log("ZoomyTrace exit");
}

//------------------------------------------------------------------------------

int main(int argc, char** argv) {
  Main m;
  m.init();

  while (!m.quit) {
    m.update();
    m.render();
  }

  m.exit();
  return 0;
}

//------------------------------------------------------------------------------