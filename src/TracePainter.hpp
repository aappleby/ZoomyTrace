#pragma once
#include <stdint.h>

#include "../symlinks/glm/glm/glm.hpp"

using namespace glm;
struct Viewport;

//-----------------------------------------------------------------------------

class TracePainter {
public:
  void init();
  void exit();

  void blit(Viewport view, dvec2 screen_size,
            int tw, int th,
            int sx, int sy, int sw, int sh,
            int dx, int dy, int dw, int dh);

  void blit(Viewport view, dvec2 screen_size, int x, int y, int w, int h);

  uint32_t trace_vao = 0;
  uint32_t trace_vbo = 0;
  uint32_t trace_ubo = 0;
  uint32_t trace_ssbo = 0;
  uint32_t trace_prog = 0;

  uint8_t* mapped_buffer = nullptr;
  size_t   mapped_len = 8*1024*1024;
};

//-----------------------------------------------------------------------------
