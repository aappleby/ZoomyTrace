#include "TraceMipper.hpp"
#include "symlinks/glad/glad.h"
#include "GLBase.h"
#include <stdio.h>
#include "log.hpp"

//------------------------------------------------------------------------------

// 512 samples x 8 bits     in
// 4x8-bit sum x 8 channels out

struct MipperUniforms {
  uint32_t samples;
  uint32_t channels;
};

const char* mipper_glsl = R"(

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

layout(std430, binding = 0) buffer Mip0 { uint mip0[]; };
layout(std430, binding = 1) buffer Mip1 { uint mip1[]; };

void main() {

  /*
  const int num_channels = 8;

  uint accum0 = 0;
  uint accum1 = 0;
  uint accum2 = 0;
  uint accum3 = 0;
  uint accum4 = 0;
  uint accum5 = 0;
  uint accum6 = 0;
  uint accum7 = 0;

  //uint cursor = gl_GlobalInvocationID.x * 128;
  for (int j = 0; j < 4; j++) {
    for (int i = 0; i < 32; i++) {
      uint bits = mip0[gl_GlobalInvocationID.x * 128 + j * 32 + i];
      accum0 += bitCount(bits & 0x01010101);
      accum1 += bitCount(bits & 0x02020202);
      accum2 += bitCount(bits & 0x04040404);
      accum3 += bitCount(bits & 0x08080808);
      accum4 += bitCount(bits & 0x10101010);
      accum5 += bitCount(bits & 0x20202020);
      accum6 += bitCount(bits & 0x40404040);
      accum7 += bitCount(bits & 0x80808080);
      //cursor++;
    }

    accum0 = (accum0 << 24) | (accum0 >> 8);
    accum1 = (accum1 << 24) | (accum1 >> 8);
    accum2 = (accum2 << 24) | (accum2 >> 8);
    accum3 = (accum3 << 24) | (accum3 >> 8);
    accum4 = (accum4 << 24) | (accum4 >> 8);
    accum5 = (accum5 << 24) | (accum5 >> 8);
    accum6 = (accum6 << 24) | (accum6 >> 8);
    accum7 = (accum7 << 24) | (accum7 >> 8);
  }

  uint cursor = gl_GlobalInvocationID.x * num_channels;
  mip1[cursor++] = accum0;
  mip1[cursor++] = accum1;
  mip1[cursor++] = accum2;
  mip1[cursor++] = accum3;
  mip1[cursor++] = accum4;
  mip1[cursor++] = accum5;
  mip1[cursor++] = accum6;
  mip1[cursor++] = accum7;
  */




  uint accum = 0;
  uint cursor = gl_GlobalInvocationID.x * 128;

  for (int j = 0; j < 4; j++) {
    for (int i = 0; i < 32; i++) {
      accum += bitCount(mip0[cursor++] & 0x01010101);
    }
    accum = (accum << 24) | (accum >> 8);
  }
  mip1[gl_GlobalInvocationID.x] = accum;



  /*
  uint t = mip0[gl_GlobalInvocationID.x];

  for (int i = 0; i < 256; i++) {
    t *= 0x1234567;
    t ^= t >> 16;
  }

  mip1[gl_GlobalInvocationID.x] = t;
  */
}

)";

//------------------------------------------------------------------------------

size_t round_up(size_t a, size_t b) {
  return ((a + b - 1) / b) * b;
}

size_t num_chunks(size_t a, size_t b) {
  return (a + b - 1) / b;
}

void TraceMipper::init() {
  mipper_prog = create_compute_shader("TraceMipper", mipper_glsl);
  mipper_ubo = create_ubo();

  //num_samples = 4 * 1024ull * 1024ull * 1024ull - 16384;
  num_samples = 1024ull*1024ull*1024ull;
  num_channels = 8;
  //mip0_size_samples = 512 * 1024 * 1024;  // this is ok
  //mip0_size_samples = 1024 * 1024 * 1024; // last element isn't getting set, lost precision somewhere

  mip0_size_bytes = num_samples;
  //mip0_ssbo = create_ssbo(mip0_size_bytes);
  //uint8_t* mip0_bytes = (uint8_t*)map_ssbo(mip0_ssbo, mip0_size_bytes);

  log("Initializing buffers");
  uint8_t* mip0_bytes = new uint8_t[num_samples];
  //for (size_t i = 0; i < mip0_size_bytes; i++) {
  //  mip0_bytes[i] = 0;
  //}
  mip0_bytes[0] = 0xFF;
  mip0_bytes[1] = 0x04;
  mip0_bytes[8] = 0x05;
  mip0_bytes[9] = 0x04;
  mip0_bytes[mip0_size_bytes - 1] = 0x11;

  log("glGenBuffers(mip0)");
  glGenBuffers(1, &mip0_ssbo);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, mip0_ssbo);
  // making the buffer DYNAMIC_STORAGE does not affect performance
  // making the buffer mappable does not affect performance
  // making the buffer persistent DOES affect performance massively (44 gs/sec -> 2 gs/sec)
  // Persistent + coherent is the same as persistent
  glBufferStorage(GL_SHADER_STORAGE_BUFFER, num_samples, mip0_bytes, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
  log("glGenBuffers(mip0) done");

  delete [] mip0_bytes;

  //unmap_ssbo(mip0_ssbo);
  //----------

  mip1_size_bytes = num_chunks(num_samples, 128) * 8;

  log("glGenBuffers(mip1)");
  glGenBuffers(1, &mip1_ssbo);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, mip1_ssbo);
  // Making this buffer persistent cuts performance to 4.3 gs/sec
  glBufferStorage(GL_SHADER_STORAGE_BUFFER, mip1_size_bytes, nullptr, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
  log("glGenBuffers(mip1) done");

  log("Initializing buffers done");

  /*
  mip1_ssbo       = create_ssbo(mip1_size_bytes);
  uint8_t* mip1_bytes = (uint8_t*)map_ssbo(mip1_ssbo, mip1_size_bytes);
  for (size_t i = 0; i < mip1_size_bytes; i++) {
    mip1_bytes[i] = 0;
  }
  unmap_ssbo(mip1_ssbo);
  */

 glGenQueries(32, queries);
}

void TraceMipper::exit() {
}

void TraceMipper::run(int , int ) {
  log("TraceMipper::run(%d, %d)", mip0_ssbo, mip1_ssbo);
  bind_compute_shader(mipper_prog);

  glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, mip0_ssbo, 0, mip0_size_bytes);
  glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 1, mip1_ssbo, 0, mip1_size_bytes);

  int work_group_size[3];
  glGetProgramiv(mipper_prog, GL_COMPUTE_WORK_GROUP_SIZE, work_group_size);

  log("Work group size X = %d", work_group_size[0]);
  log("Work group size Y = %d", work_group_size[1]);
  log("Work group size Z = %d", work_group_size[2]);

  size_t num_groups = (num_samples / 512) / work_group_size[0];
  if (num_groups == 0) num_groups = 1;

  {
    log("Dispatching 100x %ld global x %d local = %ld threads", num_groups, work_group_size[0], num_groups * work_group_size[0]);
    glBeginQuery(GL_TIME_ELAPSED, queries[1]);
    size_t reps = 100;
    for (size_t rep = 0; rep < reps; rep++) {
      glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
      glDispatchCompute(num_groups, 1, 1);
      //glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }
    glEndQuery(GL_TIME_ELAPSED);

    GLuint64 time2;
    glFinish();
    glGetQueryObjectui64v(queries[1], GL_QUERY_RESULT, &time2);
    log("Done in %f, rate %f gs/sec", double(time2) / 1.0e9, double(num_samples) / (double(time2) / reps));
  }


  glBindBuffer(GL_SHADER_STORAGE_BUFFER, mip1_ssbo);
  uint8_t* mip1_temp = (uint8_t*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, mip1_size_bytes, GL_MAP_READ_BIT);


  //glBindBuffer(GL_SHADER_STORAGE_BUFFER, mip1_ssbo);
  //uint8_t* mip1_temp = new uint8_t[mip1_size_bytes];
  //glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, mip1_size_bytes, mip1_temp);

  int cols = 32;
  int rows = mip1_size_bytes / cols;

  //uint8_t* dst = (uint8_t*)map_ssbo(mip1_ssbo, mip1_size_bytes);
  for (int y = 0; y < rows; y++) {
    if (y >= 4 && y < (rows - 4)) continue;

    printf("0x%08x: ", y * cols);
    for (int x = 0; x < cols; x++) {
      printf("%02x ", mip1_temp[y * cols + x]);
    }
    printf("\n");
  }

  glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

  //unmap_ssbo(mip1_ssbo);
  //delete [] mip1_temp;

  check_gl_error();
  log("TraceMipper::run() done");
}
