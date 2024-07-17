#include "TracePainter.hpp"

#include "GLBase.h"
#include "../glad/glad.h"
#include "ViewController.hpp"

using namespace glm;

uint32_t rng();

//-----------------------------------------------------------------------------

const char* trace_glsl = R"(

layout(std140) uniform TraceUniforms
{
  vec4 viewport;
  vec4 blit_dst_rect;
  dvec4 viewport_d;
  dvec4 blit_dst_rect_d;
  uint offset_x;
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

double remap_d(double x, double a1, double a2, double b1, double b2) {
  x = (x - a1) / (a2 - a1);
  x = x * (b2 - b1) + b1;
  return x;
}

#ifdef _VERTEX_

layout(location = 0) in  vec2 vpos;
out vec2 ftex;

void main() {

  //ftex.x = vpos.x * 2.0 * 1024.0 * 1024.0;
  ftex.x = vpos.x * 2.0 * 1024.0;
  ftex.y = 0.0;

  float dst_x = remap(vpos.x, 0.0, 1.0, blit_dst_rect.x, blit_dst_rect.z);
  float dst_y = remap(vpos.y, 0.0, 1.0, blit_dst_rect.y, blit_dst_rect.w);

  double dst_x_d = remap_d(vpos.x, 0.0, 1.0, blit_dst_rect_d.x, blit_dst_rect_d.z);
  double dst_y_d = remap_d(vpos.y, 0.0, 1.0, blit_dst_rect_d.y, blit_dst_rect_d.w);

  //gl_Position.x = remap(dst_x, viewport.x, viewport.z, -1.0,  1.0);
  //gl_Position.y = remap(dst_y, viewport.y, viewport.w,  1.0, -1.0);

  gl_Position.x = float(remap_d(dst_x_d, viewport_d.x, viewport_d.z, -1.0,  1.0));
  gl_Position.y = float(remap_d(dst_y_d, viewport_d.y, viewport_d.w,  1.0, -1.0));

  gl_Position.z = 0.0;
  gl_Position.w = 1.0;
}

#else

in  vec2 ftex;
out vec4 frag;

uniform sampler2D tex;

void main() {
  uint color = colors[uint(ftex.x) + offset_x];

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

  trace_ssbo = create_ssbo(mapped_len);

  mapped_buffer = (uint8_t*)map_ssbo(trace_ssbo, mapped_len);
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

struct TraceUniforms {
  vec4     viewport;
  vec4     blit_dst_rect;
  dvec4    viewport_d;
  dvec4    blit_dst_rect_d;
  uint32_t offset_x;
};

void TracePainter::blit(Viewport view, dvec2 screen_size,
                   int tw, int th,
                   int sx, int sy, int sw, int sh,
                   int dx, int dy, int dw, int dh) {
  TraceUniforms uniforms;

  uniforms.viewport = {
    (float)view.screen_min(screen_size).x,
    (float)view.screen_min(screen_size).y,
    (float)view.screen_max(screen_size).x,
    (float)view.screen_max(screen_size).y,
  };

  uniforms.viewport_d = {
    view.screen_min(screen_size).x,
    view.screen_min(screen_size).y,
    view.screen_max(screen_size).x,
    view.screen_max(screen_size).y,
  };

  uniforms.blit_dst_rect = {dx, dy, dx+dw, dy+dh};

  uniforms.blit_dst_rect_d = {dx, dy, dx+dw, dy+dh};
  //uniforms.offset_x = 2*1024*1024 - 128;
  uniforms.offset_x = 0;

  update_ubo(trace_ubo, sizeof(uniforms), &uniforms);

  bind_shader(trace_prog);
  bind_vao(trace_vao);
  bind_ubo(trace_prog, "TraceUniforms", 0, trace_ubo);
  bind_ssbo(trace_ssbo, 0);

  glDisable(GL_BLEND);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glEnable(GL_BLEND);
}

//-----------------------------------------------------------------------------
