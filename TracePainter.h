#pragma once
#include <stdint.h>

class TracePainter {
public:

  void init();
  void update();
  void render();


  uint32_t prog = 0;
  uint32_t tex = 0;
  uint32_t vao = 0;
  uint32_t vbo = 0;
  uint32_t ibo = 0;
};
