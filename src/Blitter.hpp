#pragma once
#include <stdint.h>

#include "third_party/glm/glm/glm.hpp"

using namespace glm;
struct Viewport;

//-----------------------------------------------------------------------------

class Blitter {
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

  uint32_t blit_vao = 0;
  uint32_t blit_vbo = 0;
  uint32_t blit_ubo = 0;
};

//-----------------------------------------------------------------------------
