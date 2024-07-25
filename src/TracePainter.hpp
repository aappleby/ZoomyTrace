#pragma once
#include <stdint.h>
#include "../symlinks/glm/glm/glm.hpp"
#include "Bits.hpp"

using namespace glm;
struct Viewport;

//-----------------------------------------------------------------------------

class TracePainter {
public:
  void init();
  void exit();

  void blit(
    Viewport view, dvec2 screen_size,
    int x, int y, int w, int h,
    TraceBuffer& trace, MipBuffer& mips, int channel);

  static constexpr int buf_count = 1;

  uint32_t trace_ubo = 0;
  uint32_t trace_prog = 0;
};

//-----------------------------------------------------------------------------
