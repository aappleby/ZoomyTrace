#include "gui.hpp"
#include "glad/glad.h"
#include "imgui/imgui.h"
#include <SDL2/SDL.h>
#include "GLBase.h"

//-----------------------------------------------------------------------------

const GLchar* imgui_glsl = R"(

uniform vec4      viewport;
uniform sampler2D tex;

float remap(float x, float a1, float a2, float b1, float b2) {
  x = (x - a1) / (a2 - a1);
  x = x * (b2 - b1) + b1;
  return x;
}

#ifdef _VERTEX_

layout (location = 0) in vec2 vert_pos;
layout (location = 1) in vec2 vert_tc;
layout (location = 2) in vec4 vert_col;

out vec2 frag_tc;
out vec4 frag_col;

void main() {
  frag_tc = vert_tc;
  frag_col = vert_col;

  float view_w = viewport.z - viewport.x;
  float view_h = viewport.w - viewport.y;

  float x = remap(vert_pos.x, viewport.x, viewport.z, -1.0, 1.0);
  float y = remap(vert_pos.y, viewport.y, viewport.w, 1.0, -1.0);

  gl_Position = vec4(x, y, 0.0, 1.0);
}

#else

in vec2 frag_tc;
in vec4 frag_col;

out vec4 frag;

void main() {
  frag = frag_col * texture(tex, frag_tc.st).rrrr;
}

#endif
)";

//------------------------------------------------------------------------------

void Gui::init() {
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGuiIO& io = ImGui::GetIO();

  //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  io.KeyMap[ImGuiKey_Tab] = SDL_SCANCODE_TAB;
  io.KeyMap[ImGuiKey_LeftArrow] = SDL_SCANCODE_LEFT;
  io.KeyMap[ImGuiKey_RightArrow] = SDL_SCANCODE_RIGHT;
  io.KeyMap[ImGuiKey_UpArrow] = SDL_SCANCODE_UP;
  io.KeyMap[ImGuiKey_DownArrow] = SDL_SCANCODE_DOWN;
  io.KeyMap[ImGuiKey_PageUp] = SDL_SCANCODE_PAGEUP;
  io.KeyMap[ImGuiKey_PageDown] = SDL_SCANCODE_PAGEDOWN;
  io.KeyMap[ImGuiKey_Home] = SDL_SCANCODE_HOME;
  io.KeyMap[ImGuiKey_End] = SDL_SCANCODE_END;
  io.KeyMap[ImGuiKey_Insert] = SDL_SCANCODE_INSERT;
  io.KeyMap[ImGuiKey_Delete] = SDL_SCANCODE_DELETE;
  io.KeyMap[ImGuiKey_Backspace] = SDL_SCANCODE_BACKSPACE;
  io.KeyMap[ImGuiKey_Space] = SDL_SCANCODE_SPACE;
  io.KeyMap[ImGuiKey_Enter] = SDL_SCANCODE_RETURN;
  io.KeyMap[ImGuiKey_Escape] = SDL_SCANCODE_ESCAPE;

  {
    imgui_prog = create_shader("imgui_glsl", imgui_glsl);

    unsigned char* pixels;
    int font_width, font_height;
    io.Fonts->GetTexDataAsAlpha8(&pixels, &font_width, &font_height);
    imgui_tex = create_texture_u8(font_width, font_height, pixels, false);

    imgui_vao = create_vao();
    imgui_vbo = create_vbo();
    imgui_ibo = create_ibo();

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(0, 2, GL_FLOAT,         GL_FALSE, 20, (void*)0);
    glVertexAttribPointer(1, 2, GL_FLOAT,         GL_FALSE, 20, (void*)8);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE,  20, (void*)16);
  }
}

//----------------------------------------

void Gui::update(SDL_Window* window) {
  ImGuiIO& io = ImGui::GetIO();
  int screen_w = 0, screen_h = 0;
  SDL_GL_GetDrawableSize((SDL_Window*)window, &screen_w, &screen_h);
  io.DisplaySize.x = float(screen_w);
  io.DisplaySize.y = float(screen_h);

  // Hax, force frame delta to be exactly the refresh interval
  SDL_DisplayMode display_mode;
  SDL_GetCurrentDisplayMode(0, &display_mode);
  io.DeltaTime = 1.0 / float(display_mode.refresh_rate);

  int mouse_x = 0, mouse_y = 0;
  int mouse_buttons    = SDL_GetMouseState(&mouse_x, &mouse_y);
  io.MouseDown[0] = (mouse_buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
  io.MouseDown[1] = (mouse_buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
  io.MouseDown[2] = (mouse_buttons & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0;
  io.MousePos = { (float)mouse_x, (float)mouse_y };
  ImGui::NewFrame();
}

//----------------------------------------

void Gui::render(SDL_Window* window) {
  ImGui::Render();

  int screen_w = 0, screen_h = 0;
  SDL_GL_GetDrawableSize((SDL_Window*)window, &screen_w, &screen_h);
  glViewport(0, 0, screen_w, screen_h);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  const ImDrawData* draw_data = ImGui::GetDrawData();
  if (draw_data->CmdListsCount) {

    glUseProgram(imgui_prog);

    glUniform4f(glGetUniformLocation(imgui_prog, "viewport"), 0, 0, (float)screen_w, (float)screen_h);
    glUniform1i(glGetUniformLocation(imgui_prog, "tex"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, imgui_tex);
    glBindVertexArray(imgui_vao);
    glBindBuffer(GL_ARRAY_BUFFER, imgui_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, imgui_ibo);

    glEnable(GL_SCISSOR_TEST);

    for (int n = 0; n < draw_data->CmdListsCount; n++) {
      const ImDrawList* l = draw_data->CmdLists[n];

      glBufferData(GL_ARRAY_BUFFER, l->VtxBuffer.Size * sizeof(ImDrawVert), l->VtxBuffer.Data, GL_STREAM_DRAW);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, l->IdxBuffer.Size * sizeof(ImDrawIdx), l->IdxBuffer.Data, GL_STREAM_DRAW);

      for (int i = 0; i < l->CmdBuffer.Size; i++) {
        const ImDrawCmd* c = &l->CmdBuffer[i];
        glScissor(int(c->ClipRect.x),
                  screen_h - int(c->ClipRect.w),
                  int(c->ClipRect.z - c->ClipRect.x),
                  int(c->ClipRect.w - c->ClipRect.y));

        glDrawElements(GL_TRIANGLES, c->ElemCount, GL_UNSIGNED_SHORT,
                        reinterpret_cast<void*>(intptr_t(c->IdxOffset * 2)));
      }
    }

    glDisable(GL_SCISSOR_TEST);
  }
}

void APIENTRY debugOutput(GLenum source, GLenum type, GLuint id, GLenum severity,
                          GLsizei length, const GLchar* message, const GLvoid* userParam);

SDL_Window* create_gl_window(const char* name, int initial_screen_w, int initial_screen_h) {
  SDL_Init(SDL_INIT_EVERYTHING);
  //SDL_Init(SDL_INIT_VIDEO);

  SDL_Window* window = SDL_CreateWindow(
    name,
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    1920, 1080,
    SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI
  );

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,
                      SDL_GL_CONTEXT_DEBUG_FLAG
                      //| SDL_GL_CONTEXT_ROBUST_ACCESS_FLAG
                      //| SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG
                      );

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);

  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  SDL_GLContext gl_context = SDL_GL_CreateContext((SDL_Window*)window);

  SDL_GL_SetSwapInterval(1);  // Enable vsync
  //SDL_GL_SetSwapInterval(0); // Disable vsync

  gladLoadGLLoader(SDL_GL_GetProcAddress);

  glEnable(GL_DEBUG_OUTPUT);
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
  glDebugMessageCallback(debugOutput, nullptr);

  int ext_count = 0;
  glGetIntegerv(GL_NUM_EXTENSIONS, &ext_count);

  printf("Vendor:   "); printf("%s\n", glGetString(GL_VENDOR));
  printf("Renderer: "); printf("%s\n", glGetString(GL_RENDERER));
  printf("Version:  "); printf("%s\n", glGetString(GL_VERSION));
  printf("GLSL:     "); printf("%s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
  printf("Ext count "); printf("%d\n", ext_count);

#if 0
  for (int i = 0; i < ext_count; i++) {
    LOG_B("Ext %2d: %s\n", i, glGetStringi(GL_EXTENSIONS, i));
  }
#endif

  //----------------------------------------
  // Set initial GL state

  glDisable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glClearColor(0.1, 0.1, 0.2, 0.0);
  glClearDepth(1.0);

  return window;
}
