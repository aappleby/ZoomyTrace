#include "TracePainter.hpp"

#include "GLBase.h"
#include "../glad/glad.h"
#include "ViewController.hpp"

using namespace glm;

uint32_t rng();

constexpr int buf_size = 8*1024*1024;

//-----------------------------------------------------------------------------

struct TraceUniforms {
  vec4 viewport;
  vec4 blit_dst_rect;
};

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

  ftex.x = vpos.x * 2.0 * 1024.0 * 1024.0;
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

  trace_ssbo = create_ssbo(buf_size);

  mapped_buffer = (uint32_t*)map_ssbo(trace_ssbo, buf_size);
  for (int i = 0; i < 2048; i++) {
    auto t = i & 0xFF;
    mapped_buffer[i] = 0xFF000000 | (t << 0) | (t << 8) | (t << 16);
  }

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

  trace_prog = create_shader("trace_glsl", trace_glsl);
}

void TracePainter::exit() {
  unmap_ssbo(trace_ssbo);
}

//-----------------------------------------------------------------------------

void TracePainter::blit(Viewport view, dvec2 screen_size, int x, int y, int w, int h) {
  blit(view, screen_size, w, h, 0, 0, w, h, x, y, w, h);
}

//-----------------------------------------------------------------------------

void TracePainter::blit(Viewport view, dvec2 screen_size,
                   int tw, int th,
                   int sx, int sy, int sw, int sh,
                   int dx, int dy, int dw, int dh) {
  TraceUniforms blit_uniforms;

  blit_uniforms.viewport = {
    (float)view.screen_min(screen_size).x,
    (float)view.screen_min(screen_size).y,
    (float)view.screen_max(screen_size).x,
    (float)view.screen_max(screen_size).y,
  };

  blit_uniforms.blit_dst_rect = {dx, dy, dx+dw, dy+dh};

  update_ubo(trace_ubo, sizeof(blit_uniforms), &blit_uniforms);

  bind_shader(trace_prog);
  bind_vao(trace_vao);
  bind_ubo(trace_prog, "TraceUniforms", 0, trace_ubo);
  bind_ssbo(trace_ssbo, 0);

  glDisable(GL_BLEND);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glEnable(GL_BLEND);
}

//-----------------------------------------------------------------------------
