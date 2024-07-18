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

  void blit(Viewport view, dvec2 screen_size, int x, int y, int w, int h);

  uint32_t trace_vao = 0;
  uint32_t trace_vbo = 0;
  uint32_t trace_ubo = 0;
  uint32_t trace_prog = 0;

  uint32_t trace_ssbos[3];

  uint8_t* mapped_buffers[3];
  size_t   mapped_len = 8*1024*1024;
};

//-----------------------------------------------------------------------------
