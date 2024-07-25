#include "main.hpp"

#include "Bits.hpp"
#include "capture.hpp"
#include "GLBase.h"
#include "log.hpp"

#include "symlinks/glad/glad.h"
#include "symlinks/imgui/imgui.h"
#include <SDL2/SDL.h>
#include <algorithm>

void log(const char* format, ...);
void err(const char* format, ...);

extern std::string log_buf;

//------------------------------------------------------------------------------

void gen_pattern(void* buf, size_t sample_count) {

  uint8_t* bits = (uint8_t*)buf;

  uint32_t x = 1;
  for (size_t i = 0; i < sample_count; i++) {

    //bits[i + 0] = 0b00000001;
    //bits[i + 1] = 0b00000000;
    //bits[i + 2] = 0b00000000;
    //bits[i + 3] = 0b01111110;

    //bits[i] = i;

    size_t t = i;
    t = t*((t>>9|t>>13)&25&t>>6);
    bits[i] = t;

    //size_t t = i;
    //t *= 0x23456789;
    //t ^= (t >> 45);
    //t *= 0x23456789;
    //t ^= (t >> 45);
    //bits[i] = t;

    //bits[i] = rng();
    //bits[i] = i >> 24;
  }

}

//------------------------------------------------------------------------------

void Main::init() {
  log("ZoomyTrace init");

  double time_a, time_b;

  //ring = new RingBuffer(8ull * 1024ull * 1024ull);
  //ring = new RingBuffer(64ull * 1024ull);

  //memset(ring->buffer, 0, ring->buffer_len);

  cap = new Capture();
  cap->start_thread();

  int initial_screen_w, initial_screen_h;
  initial_screen_w = 1920;
  initial_screen_h = 1080;
  window = create_gl_window("Logicdump", initial_screen_w, initial_screen_h);

  //----------
  // GL up

  //

  {
    int wgc_x = 0;
    int wgc_y = 0;
    int wgc_z = 0;
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &wgc_x);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &wgc_y);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &wgc_z);
    log("Workgroup count %d %d %d", wgc_x, wgc_y, wgc_z);

    int wgs_x = 0;
    int wgs_y = 0;
    int wgs_z = 0;
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &wgs_x);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &wgs_y);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &wgs_z);
    log("Workgroup size %d %d %d", wgs_x, wgs_y, wgs_z);

    int wg_inv = 0;
    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &wg_inv);
    log("Workgroup invocations %d", wg_inv);
  }


  //static constexpr size_t sample_count  = 64ull*1024ull;
  //static constexpr size_t channel_count = 8;

  //trace.samples  = 4ull*1024ull*1024ull*1024ull - (128*128*128*128);
  //trace.samples  = 128*128*128*128;
  trace.samples  = 65536;
  trace.channels = 8;
  trace.stride   = 8;
  trace.ssbo_len = (trace.samples * trace.stride + 7) / 8;
  trace.ssbo     = create_ssbo(trace.ssbo_len);
  trace.blob     = map_ssbo(trace.ssbo, trace.ssbo_len);

  blit.init();
  trace_painter.init();

  trace_mipper.init();
  trace_mipper.run(0, 0);

  uint8_t* pattern = new uint8_t[trace.ssbo_len];
  log("generating pattern");
  time_a = timestamp();
  gen_pattern(pattern, trace.ssbo_len);
  time_b = timestamp();
  log("generating pattern done in %12.8f sec", time_b - time_a);

  time_a = timestamp();
  memcpy(trace.blob, pattern, trace.ssbo_len);
  time_b = timestamp();
  log("pattern copied to trace ssbo in %f", time_b - time_a);

  size_t mip1_len = (trace.samples + 127) / 128;
  size_t mip2_len = (mip1_len + 127) / 128;
  size_t mip3_len = (mip2_len + 127) / 128;
  size_t mip4_len = (mip3_len + 127) / 128;

  log("mip chain %ld %ld %ld %ld", mip1_len, mip2_len, mip3_len, mip4_len);

  size_t mip1_align = (mip1_len + 127) & ~127;
  size_t mip2_align = (mip2_len + 127) & ~127;
  size_t mip3_align = (mip3_len + 127) & ~127;
  size_t mip4_align = (mip4_len + 127) & ~127;

  size_t mip_total = mip1_align + mip2_align + mip3_align + mip4_align;

  time_a = timestamp();
  for (int i = 0; i < 8; i++) {
    mips[i].samples = trace.samples;

    mips[i].ssbo_len = mip_total;
    mips[i].ssbo = create_ssbo(mip_total);

    mips[i].mip1 = (uint8_t*)map_ssbo(mips[i].ssbo, mips[i].ssbo_len);
    mips[i].mip2 = mips[i].mip1 + mip1_align;
    mips[i].mip3 = mips[i].mip2 + mip2_align;
    mips[i].mip4 = mips[i].mip3 + mip3_align;

    mips[i].mip1_len = mip1_len;
    mips[i].mip2_len = mip2_len;
    mips[i].mip3_len = mip3_len;
    mips[i].mip4_len = mip4_len;

    mips[i].mip1_offset = 0;
    mips[i].mip2_offset = mip1_align;
    mips[i].mip3_offset = mip1_align + mip2_align;
    mips[i].mip4_offset = mip1_align + mip2_align + mip3_align;

    auto old_blob = trace.blob;
    trace.blob = pattern;
    update_mips(trace, i, 0, trace.samples, mips[i]);
    trace.blob = old_blob;
  }
  time_b = timestamp();
  log("generating mips done in %12.8f sec", time_b - time_a);

  vcon.init({initial_screen_w, initial_screen_h});

  //----------------------------------------
  // Initialize ImGui and ImGui renderer

  gui.init();

  old_now = timestamp();
  new_now = timestamp();
  frame = -1;

  delete [] pattern;
}

//------------------------------------------------------------------------------

void Main::exit() {
  unmap_ssbo(trace.ssbo);
  cap->stop_thread();
  delete cap;
  trace_painter.exit();
  log("ZoomyTrace exit");
}

//------------------------------------------------------------------------------

void Main::update() {

  SDL_GL_GetDrawableSize((SDL_Window*)window, &screen_w, &screen_h);
  screen_size = { (double)screen_w, (double)screen_h };

  mouse_buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
  dvec2 mouse_pos_screen = { (double)mouse_x, (double)mouse_y };

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

    // !imgui_io.WantCaptureKeyboard && !imgui_io.WantTextInput

    if (event.type == SDL_KEYDOWN) {
      if (event.key.keysym.sym == SDLK_ESCAPE) {
        if (event.key.keysym.mod & KMOD_LSHIFT) {
          quit = true;
        } else {
          vcon.view_target      = Viewport(screen_size * 0.5, {0, 0});
          vcon.view_target_snap = Viewport(screen_size * 0.5, {0, 0});
        }
      }
      else if (event.key.keysym.sym == SDLK_MINUS) {
        vcon.zoom(mouse_pos_screen, screen_size, -0.25, 0);
      }
      else if (event.key.keysym.sym == SDLK_EQUALS) {
        vcon.zoom(mouse_pos_screen, screen_size, 0.25, 0);
      }
    }

    if (event.type == SDL_MOUSEWHEEL && !imgui_io.WantCaptureMouse) {
      vcon.zoom(mouse_pos_screen, screen_size, double(event.wheel.y) * zoom_per_tick, 0);
    }

    if (event.type == SDL_MOUSEMOTION && !imgui_io.WantCaptureMouse) {
      if (event.motion.state & SDL_BUTTON_LMASK) {
        dvec2 rel = { (double)event.motion.xrel, (double)event.motion.yrel };
        vcon.pan(rel);
      }
    }
  }

  //----------------------------------------

  while (!cap->cap_to_host.empty()) {
    auto res = cap->cap_to_host.get();
    log("<- %-16s 0x%08x 0x%016x %ld", capcmd_to_cstr(res.command), res.result, res.block, res.length);
  }

  double delta = new_now - old_now;
  vcon.update(delta);

  update_imgui();
}

//------------------------------------------------------------------------------

void Main::update_imgui() {
  ImGui::NewFrame();

  ImGui::Begin("Viewport");
  {
    auto w1 = vcon.view_target._center;
    auto z1 = vcon.view_target._zoom;
    auto w2 = vcon.view_target_snap._center;
    auto z2 = vcon.view_target_snap._zoom;
    auto w3 = vcon.view_smooth._center;
    auto z3 = vcon.view_smooth._zoom;
    auto w4 = vcon.view_smooth_snap._center;
    auto z4 = vcon.view_smooth_snap._zoom;
    ImGui::Text("view target      %f %f %f %f\n", w1.x, w1.y, z1.x, z1.y);
    ImGui::Text("view target snap %f %f %f %f\n", w2.x, w2.y, z2.x, z2.y);
    ImGui::Text("view smooth      %f %f %f %f\n", w3.x, w3.y, z3.x, z3.y);
    ImGui::Text("view smooth snap %f %f %f %f\n", w4.x, w4.y, z4.x, z4.y);

    dvec2 world_min = vcon.view_target.world_min(screen_size);
    dvec2 world_max = vcon.view_target.world_max(screen_size);

    ImGui::Text("view width       %f\n",world_max.x - world_min.x);
  }
  ImGui::End();

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
    //ImGui::Text("ring_buffer     %p", ring->buffer);
    //ImGui::Text("ring_size       %ld", ring->buffer_len);
    //ImGui::Text("ring_chunk      %d", ring->chunk_size);

    ImGui::Text("ring_read       %d", (int)cursor_read );
    ImGui::Text("ring_ready      %d", (int)cursor_ready);
    ImGui::Text("ring_write      %d", (int)cursor_write);

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
      cap->post_async({XCMD_START_CAP, 1024, 0, 0});
    }

    if (ImGui::Button("stop_cap", {100,25})) {
      cap->post_async({XCMD_STOP_CAP, 0, 0, 0});
    }
  }

  ImGui::Text("frame      %06d", frame);
  ImGui::Text("frame rate %f", 1.0 / (new_now - old_now));
  ImGui::Text("frame time %f", new_now - old_now);
  ImGui::Text("render time %f", render_time);

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

double* temp = nullptr;

void Main::render() {
  if (temp == nullptr) {
    temp = new double[1024*1024*8];
  }

  glViewport(0, 0, screen_w, screen_h);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  /*
  for (int i = 0; i < 8; i++) {
    mips[i] = new BitMips();
    mips[i]->update_mips(*blob, i, 0, blob->sample_count);
  }
  */

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, trace.ssbo);
  glFlushMappedBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 1024);

  auto time_a = timestamp();
  int cursor_y = 64;
  for (int channel = 0; channel < 8; channel++) {
    trace_painter.blit(
      vcon.view_smooth_snap, screen_size,
      0, cursor_y, 1920, 64,
      trace, mips[channel], channel);
    cursor_y += 96;
  }
  auto time_b = timestamp();
  render_time = time_b - time_a;

  gui.render_gl(window);

  SDL_GL_SwapWindow((SDL_Window*)window);
}

//------------------------------------------------------------------------------

int main(int argc, char** argv) {
  Main m;
  m.init();

  //while (!m.quit) {
  //  m.update();
  //  m.render();
  //}

  m.exit();
  return 0;
}

//------------------------------------------------------------------------------
