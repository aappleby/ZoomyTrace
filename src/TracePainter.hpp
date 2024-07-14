#pragma once
#include <stdint.h>

#include "../symlinks/glm/glm/glm.hpp"

using namespace glm;
struct Viewport;

//-----------------------------------------------------------------------------

class TracePainter {
public:
  void init();

  void blit(Viewport view, dvec2 screen_size,
            uint32_t tex, int tw, int th,
            int sx, int sy, int sw, int sh,
            int dx, int dy, int dw, int dh);

  void blit(Viewport view, dvec2 screen_size, uint32_t tex, int x, int y, int w, int h);

  void blit_mono(Viewport view, dvec2 screen_size,
                 uint32_t tex, int tw, int th,
                 int sx, int sy, int sw, int sh,
                 int dx, int dy, int dw, int dh);

  uint32_t trace_vao = 0;
  uint32_t trace_vbo = 0;
  uint32_t trace_ubo = 0;
  uint32_t trace_ssbo = 0;
  uint32_t trace_prog = 0;
};

//-----------------------------------------------------------------------------
