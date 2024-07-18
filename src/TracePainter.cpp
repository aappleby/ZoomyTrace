#include "TracePainter.hpp"

#include "GLBase.h"
#include "symlinks/glad/glad.h"
#include "ViewController.hpp"
#include <stdio.h>

using namespace glm;

uint32_t rng();

//-----------------------------------------------------------------------------

const char* trace_glsl = R"(

layout(std140) uniform TraceUniforms
{
  vec4 viewport;
  vec4 blit_dst_rect;
};

layout(std430, binding = 0) buffer TraceBuffer
{
  uint colors[];
};

float remap(float x, float a1, float a2, float b1, float b2) {
  x = (x - a1) / (a2 - a1);
  x = x * (b2 - b1) + b1;
  return x;
}

#ifdef _VERTEX_

layout(location = 0) in  vec2 vpos;
out vec2 ftex;

void main() {

  //ftex.x = vpos.x * 2.0 * 1024.0 * 1024.0;
  ftex.x = vpos.x * 1920.0;
  ftex.y = 0.0;

  float dst_x = remap(vpos.x, 0.0, 1.0, blit_dst_rect.x, blit_dst_rect.z);
  float dst_y = remap(vpos.y, 0.0, 1.0, blit_dst_rect.y, blit_dst_rect.w);

  gl_Position.x = remap(dst_x, viewport.x, viewport.z, -1.0,  1.0);
  gl_Position.y = remap(dst_y, viewport.y, viewport.w,  1.0, -1.0);

  gl_Position.z = 0.0;
  gl_Position.w = 1.0;
}

#else

in  vec2 ftex;
out vec4 frag;

uniform sampler2D tex;

void main() {
  uint color = colors[uint(ftex.x)];

  frag = unpackUnorm4x8(color);
}

#endif

)";

//-----------------------------------------------------------------------------

void TracePainter::init() {

  float unit_quad[] = {
    0, 0,
    1, 0,
    0, 1,
    1, 1,
  };

  trace_ubo = create_ubo();
  trace_vao = create_vao();
  trace_vbo = create_vbo(sizeof(unit_quad), unit_quad);

  for (int i = 0; i < 3; i++) {
    trace_ssbos[i] = create_ssbo(mapped_len);
    mapped_buffers[i] = (uint8_t*)map_ssbo(trace_ssbos[i], mapped_len);
    for (int j = 0; j < 2048; j++) {
      auto t = j & 0xFF;
      mapped_buffers[i][j] = 0xFF000000 | (t << 0) | (t << 8) | (t << 16);
    }
  }

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

  trace_prog = create_shader("trace_glsl", trace_glsl);
}

void TracePainter::exit() {
  for (int i = 0; i < 3; i++) {
    unmap_ssbo(trace_ssbos[i]);
  }
}

//-----------------------------------------------------------------------------

struct TraceUniforms {
  vec4     viewport;
  vec4     blit_dst_rect;
};

void TracePainter::blit(Viewport view, dvec2 screen_size, int x, int y, int w, int h) {

  TraceUniforms uniforms;

  uniforms.viewport = {
    (float)view.world_min(screen_size).x,
    (float)view.world_min(screen_size).y,
    (float)view.world_max(screen_size).x,
    (float)view.world_max(screen_size).y,
  };

  uniforms.blit_dst_rect = {x, y, x+w, y+h};

  update_ubo(trace_ubo, sizeof(uniforms), &uniforms);

  bind_shader(trace_prog);
  bind_vao(trace_vao);
  bind_ubo(trace_prog, "TraceUniforms", 0, trace_ubo);
  bind_ssbo(trace_ssbos[0], 0);

  glDisable(GL_BLEND);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glEnable(GL_BLEND);

  auto temp_ssbo = trace_ssbos[0];
  trace_ssbos[0] = trace_ssbos[1];
  trace_ssbos[1] = trace_ssbos[2];
  trace_ssbos[2] = temp_ssbo;

  auto temp_buf  = mapped_buffers[0];
  mapped_buffers[0] = mapped_buffers[1];
  mapped_buffers[1] = mapped_buffers[2];
  mapped_buffers[2] = temp_buf;
}

//-----------------------------------------------------------------------------
