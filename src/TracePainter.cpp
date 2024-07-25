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
  float  blit_x;
  float  blit_y;
  float  blit_w;
  float  blit_h;
  vec4   screen_size;
  double scale;
  double offset;
  uint   stride;
  uint   channel;
  int    miplevel;
};

layout(std430, binding = 0) buffer Mip0 { uint mip0[]; };
layout(std430, binding = 1) buffer Mip1 { uint mip1[]; };
layout(std430, binding = 2) buffer Mip2 { uint mip2[]; };
layout(std430, binding = 3) buffer Mip3 { uint mip3[]; };
layout(std430, binding = 4) buffer Mip4 { uint mip4[]; };

float remap(float x, float a1, float a2, float b1, float b2) {
  x = (x - a1) / (a2 - a1);
  x = x * (b2 - b1) + b1;
  return x;
}

//------------------------------------------------------------------------------

#ifdef _VERTEX_

out vec2 ftex;

void main() {

  float vpos_x = float((gl_VertexID >> 0) & 1);
  float vpos_y = float((gl_VertexID >> 1) & 1);

  float screen_x = vpos_x * blit_w + blit_x;
  float screen_y = vpos_y * blit_h + blit_y;

  float norm_x = (screen_x * screen_size.z) * 2.0 - 1.0;
  float norm_y = (screen_y * screen_size.w) * 2.0 - 1.0;

  gl_Position = vec4(norm_x, -norm_y, 0.0, 1.0);
}

#endif

//------------------------------------------------------------------------------

#ifdef _FRAGMENT_

out vec4 frag;

void main() {
  double x = double(gl_FragCoord.x) * scale + offset;

  if (x < 0.0) {
    frag = vec4(0,0,0.2,1);
    return;
  }
  if (x >= 4294967296.0) {
    frag = vec4(0,0,0.2,1);
    return;
  }

  x = floor(x);

  if (miplevel == 0) {
    double stride_index = x * stride + channel;
    double word_index   = (stride_index) / 32.0;
    double bit_index    = mod(stride_index, 32.0);

    uint bit = bitfieldExtract(mip0[int(word_index)], int(bit_index), 1);

    frag = bit == 1 ? vec4(1,1,1,1) : vec4(0,0,0,1);
  }
  else if (miplevel == 1) {
    x /= 128;
    double word_index = x / 4;
    double byte_index = mod(x, 4);
    uint byte = bitfieldExtract(mip1[int(word_index)], int(byte_index) * 8, 8);

    float t = float(byte) / 255.0;
    frag = vec4(t, t, t, 1);
  }
  else if (miplevel == 2) {
    x /= 128 * 128;
    double word_index = x / 4;
    double byte_index = mod(x, 4);
    uint byte = bitfieldExtract(mip2[int(word_index)], int(byte_index) * 8, 8);

    float t = float(byte) / 255.0;
    frag = vec4(t, t, t, 1);
  }
  else if (miplevel == 3) {
    x /= 128 * 128 * 128;
    double word_index = x / 4;
    double byte_index = mod(x, 4);
    uint byte = bitfieldExtract(mip3[int(word_index)], int(byte_index) * 8, 8);

    float t = float(byte) / 255.0;
    frag = vec4(t, t, t, 1);
  }
  else if (miplevel == 4) {
    x /= 128 * 128 * 128 * 128;
    double word_index = x / 4;
    double byte_index = mod(x, 4);
    uint byte = bitfieldExtract(mip4[int(word_index)], int(byte_index) * 8, 8);

    float t = float(byte) / 255.0;
    frag = vec4(t, t, t, 1);
  }
  else {
    frag = vec4(0, 0.2, 0.2, 1);
  }
}

#endif

//------------------------------------------------------------------------------

)";

//-----------------------------------------------------------------------------

template<typename T>
void rotate_bufs(T* bufs, int count) {
  auto temp = bufs[0];
  for (int i = 0; i < count; i++) {
    bufs[i] = i == count - 1 ? temp : bufs[i + 1];
  }
}

//-----------------------------------------------------------------------------

void TracePainter::init() {

  trace_ubo = create_ubo();

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

  trace_prog = create_shader("trace_glsl", trace_glsl);
}

void TracePainter::exit() {
}

//-----------------------------------------------------------------------------

struct TraceUniforms {
  float blit_x;
  float blit_y;
  float blit_w;
  float blit_h;
  vec4     screen_size;
  double   scale;
  double   offset;
  uint32_t stride;
  uint32_t channel;
  int32_t  miplevel;
};

void TracePainter::blit(
  Viewport view, dvec2 screen_size,
  int x, int y, int w, int h,
  TraceBuffer& trace, MipBuffer& mips, int channel) {

  TraceUniforms uniforms;

  dvec2 world_min = view.world_min(screen_size);
  dvec2 world_max = view.world_max(screen_size);

  uniforms.blit_x = x;
  uniforms.blit_y = y;
  uniforms.blit_w = w;
  uniforms.blit_h = h;
  uniforms.screen_size = { screen_size.x, screen_size.y, 1.0 / screen_size.x, 1.0 / screen_size.y };

  uniforms.scale  = (world_max.x - world_min.x) / screen_size.x;
  uniforms.offset = world_min.x;

  //uniforms.scale = { screen_size.x, 1.0 };
  //uniforms.offset_coarse = 3000000000;
  //uniforms.offset_fine   = 0;

  uniforms.channel = channel;
  uniforms.stride = trace.stride;
  uniforms.miplevel = (-view._zoom.x) / 7;

  update_ubo(trace_ubo, sizeof(uniforms), &uniforms);

  bind_shader(trace_prog);
  bind_ubo(trace_prog, "TraceUniforms", 0, trace_ubo);

  bind_ssbo(trace.ssbo, 0, 0, trace.ssbo_len);
  bind_ssbo(mips.ssbo,  1, mips.mip1_offset, mips.mip1_len);
  bind_ssbo(mips.ssbo,  2, mips.mip2_offset, mips.mip2_len);
  bind_ssbo(mips.ssbo,  3, mips.mip3_offset, mips.mip3_len);
  bind_ssbo(mips.ssbo,  4, mips.mip4_offset, mips.mip4_len);

  /*
  {
    int64_t ssbo_binding;
    int64_t ssbo_align;
    int64_t ssbo_start;
    int64_t ssbo_size;

    glGetInteger64v(GL_SHADER_STORAGE_BUFFER_BINDING,          &ssbo_binding);
    glGetInteger64v(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &ssbo_align);
    glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_START,            0, &ssbo_start);
    glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_SIZE,             0, &ssbo_size);

    printf("ssbo_binding %ld\n", ssbo_binding);
    printf("ssbo_align   %ld\n", ssbo_align);
    printf("ssbo_start   %ld\n", ssbo_start);
    printf("ssbo_size    %ld\n", ssbo_size);
  }
  */

  glDisable(GL_BLEND);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glEnable(GL_BLEND);

  //rotate_bufs(trace_ssbos, buf_count);
  //rotate_bufs(mapped_buffers, buf_count);
}

//-----------------------------------------------------------------------------
