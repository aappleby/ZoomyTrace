#include "TraceMipper.hpp"
#include "symlinks/glad/glad.h"
#include "GLBase.h"
#include <stdio.h>
#include "log.hpp"

// making mip0 DYNAMIC_STORAGE does not affect performance
// making mip0 mappable does not affect performance
// making mip0 persistent DOES affect performance massively (44 gs/sec -> 2 gs/sec)
// Persistent + coherent is the same as persistent

//------------------------------------------------------------------------------
// 64-bit version is 50% faster than 32-bit

// Each invocation of this shader converts 128 bytes of bit-interleaved
// 8-channel sample data into 8 8-bit accumulators, where accumulator N stores
// how many times bit N was set in each byte of the source data. The output
// is 8-channel striped accumulators.

const char* mipper_glsl_64 = R"(

layout(std430, binding = 0) buffer Mip0 { uint64_t mip0[]; };
layout(std430, binding = 1) buffer Mip1 { uint64_t mip1[]; };

void main() {

  // Our accumulator is actually 8 8-bit accumulators in one variable.
  uint64_t accum = 0;

  // We process mip0 in 128-sample chunks, or 16 8-sample chunks.
  for (int j = 0; j < 16; j++) {
    uint64_t chunk = mip0[gl_GlobalInvocationID.x * 16 + j];

    // Reverse the bits so that channels will be in order after expansion.
    // Somehow this also makes the shader a few percent faster...?

    uint chunk_lo = uint(chunk >> 0);
    uint chunk_hi = uint(chunk >> 32);
    chunk = uint64_t(bitfieldReverse(chunk_lo) << 32) | uint64_t(bitfieldReverse(chunk_hi) << 0);

    for (int i = 0; i < 8; i++) {
      // Grab a byte of the chunk and 'expand' it out via multiplication. This
      // will place each bit of the byte at index 8n+7 in 'expanded'
      uint64_t expanded = ((chunk >> (8*i)) & 0xFF) * 0x8040201008040201L;

      // Shift the expanded bits down to index 0 and add them all to the accumulator in parallel.
      accum += (expanded >> 7) & 0x0101010101010101L;
    }
  }

  // Done, store all 8 channels of the accumulator in the output buffer
  mip1[gl_GlobalInvocationID.x] = accum;
}
)";

//------------------------------------------------------------------------------
// 32-bit version

const char* mipper_glsl_32 = R"(

layout(std430, binding = 0) buffer Mip0 { uint mip0[]; };
layout(std430, binding = 1) buffer Mip1 { uint mip1[]; };

void main() {
  uint cursor = gl_GlobalInvocationID.x * 32;
  uint accum_lo = 0;
  uint accum_hi = 0;

  for (int j = 0; j < 32; j++) {
    uint chunk = mip0[cursor++];

    for (int i = 0; i < 4; i++) {
      uint expanded_lo = (chunk & 0x0F) * 0x80402010;
      accum_lo += (expanded_lo >> 7) & 0x01010101;
      chunk >>= 4;

      uint expanded_hi = (chunk & 0x0F) * 0x80402010;
      accum_hi += (expanded_hi >> 7) & 0x01010101;
      chunk >>= 4;
    }
  }

  mip1[gl_GlobalInvocationID.x * 2 + 0] = accum_lo;
  mip1[gl_GlobalInvocationID.x * 2 + 1] = accum_hi;
}

)";

//------------------------------------------------------------------------------
// Each invocation of this shader converts 1024 bytes of 8-channel striped
// accumulator data into 8 bytes of merged accumulator data, where the Nth
// merged accumulator stores the average value (rounded up) of the Nth stripe
// of the source data. The output is also 8-channel striped.

const char* merger_glsl_64 = R"(

layout(std430, binding = 0) buffer Mip1 { uint64_t mip1[]; };
layout(std430, binding = 1) buffer Mip2 { uint64_t mip2[]; };

void main() {

  uint64_t accum_lo = 0;
  uint64_t accum_hi = 0;

  for (int j = 0; j < 128; j++) {
    uint64_t chunk = mip1[gl_GlobalInvocationID.x * 128 + j];
    accum_lo += ((chunk & 0x00FF00FF00FF00FFL) >> 0);
    accum_hi += ((chunk & 0xFF00FF00FF00FF00L) >> 8);
  }

  // Force accumulators to round up
  accum_lo += 0x007F007F007F007FL;
  accum_hi += 0x007F007F007F007FL;

  // Scale down by 128
  accum_lo = (accum_lo >> 7) & 0x00FF00FF00FF00FFL;
  accum_hi = (accum_hi >> 7) & 0x00FF00FF00FF00FFL;

  // Splice back together
  uint64_t accum = accum_lo | (accum_hi << 8);

  // Done, store the merged accumulator.
  mip2[gl_GlobalInvocationID.x] = accum;
}

)";

//------------------------------------------------------------------------------

size_t round_up(size_t a, size_t b) {
  return ((a + b - 1) / b) * b;
}

size_t num_chunks(size_t a, size_t b) {
  return (a + b - 1) / b;
}

//------------------------------------------------------------------------------

void TraceMipper::init() {
  log("TraceMipper::init()");

  // Max size, any larger than this cuts off the end for some reason
  //num_samples = 4 * 1024ull * 1024ull * 1024ull - 4096;
  num_samples = 256*1024ull*1024ull;
  num_channels = 8;

  mipper_prog = create_compute_shader("TraceMipper", mipper_glsl_64);
  merger_prog = create_compute_shader("TraceMerger", merger_glsl_64);
  mipper_ubo = create_ubo();

  log("Initializing buffers");

  mip0_size_bytes = num_samples;

  log("glGenBuffers(mip0) %ld", mip0_size_bytes);
  glGenBuffers(1, &mip0_ssbo);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, mip0_ssbo);
  glBufferStorage(GL_SHADER_STORAGE_BUFFER, mip0_size_bytes, nullptr, GL_DYNAMIC_STORAGE_BIT);
  log("glGenBuffers(mip0) done");

  // Put some test data in mip0
  {
    uint8_t* buf = new uint8_t[mip0_size_bytes];

    for (size_t i = 0; i < mip0_size_bytes; i++) {
      buf[i] = rng();
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, mip0_ssbo);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, mip0_size_bytes, buf);
    delete [] buf;
  }

  //----------

  mip1_size_bytes = num_chunks(num_samples, 128) * 8;

  log("glGenBuffers(mip1) %ld", mip1_size_bytes);
  glGenBuffers(1, &mip1_ssbo);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, mip1_ssbo);
  // Making this buffer persistent cuts performance to 4.3 gs/sec
  glBufferStorage(GL_SHADER_STORAGE_BUFFER, mip1_size_bytes, nullptr, GL_DYNAMIC_STORAGE_BIT);
  log("glGenBuffers(mip1) done");

  // Put some test data in mip1
  if (0) {
    uint8_t* buf = new uint8_t[mip1_size_bytes];
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, mip1_ssbo);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, mip1_size_bytes, buf);
    delete [] buf;
  }

  //----------

  mip2_size_bytes = num_chunks(mip1_size_bytes, 128);

  log("glGenBuffers(mip2) %ld", mip2_size_bytes);
  glGenBuffers(1, &mip2_ssbo);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, mip2_ssbo);
  // Making this buffer persistent cuts performance to 4.3 gs/sec
  glBufferStorage(GL_SHADER_STORAGE_BUFFER, mip2_size_bytes, nullptr, GL_DYNAMIC_STORAGE_BIT);
  log("glGenBuffers(mip2) done");

  log("Initializing buffers done");

 glGenQueries(32, queries);
}

//------------------------------------------------------------------------------

void TraceMipper::exit() {
}

//------------------------------------------------------------------------------
// Dump out the first and last 4 rows of mip1 for debugging

void dump_ssbo(int ssbo, size_t size_bytes) {

  size_t cols = 8;
  size_t rows = size_bytes / cols;

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);

  uint8_t* buf = new uint8_t[size_bytes];
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, size_bytes, buf);

  for (size_t y = 0; y < rows; y++) {
    if (y >= 4 && y < (rows - 4)) continue;

    printf("0x%08lx: ", y * cols);
    for (size_t x = 0; x < cols; x++) {
      printf("%02x ", buf[y * cols + x]);
    }
    printf("\n");
  }

  delete [] buf;
}

//------------------------------------------------------------------------------

void TraceMipper::run(int , int ) {
  log("TraceMipper::run(%d, %d)", mip0_ssbo, mip1_ssbo);

  //----------------------------------------
  // Run the mipper

  {
    bind_compute_shader(mipper_prog);
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, mip0_ssbo, 0, mip0_size_bytes);
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 1, mip1_ssbo, 0, mip1_size_bytes);

    int work_group_size[3];
    glGetProgramiv(mipper_prog, GL_COMPUTE_WORK_GROUP_SIZE, work_group_size);

    log("Work group size X = %d", work_group_size[0]);
    log("Work group size Y = %d", work_group_size[1]);
    log("Work group size Z = %d", work_group_size[2]);

    size_t num_groups = (mip0_size_bytes / 128) / work_group_size[0];
    if (num_groups == 0) num_groups = 1;

    //----------------------------------------
    // Warmup

    log("Warmup");
    for (size_t rep = 0; rep < 100; rep++) {
      glDispatchCompute(num_groups, 1, 1);
    }

    //----------------------------------------
    // Benchmark

    for (size_t top_rep = 0; top_rep < 4; top_rep++) {

      size_t reps = 100;
      log("Dispatching %dx %ld global x %d local = %ld threads", reps, num_groups, work_group_size[0], num_groups * work_group_size[0]);

      glFinish();
      glBeginQuery(GL_TIME_ELAPSED, queries[1]);
      for (size_t rep = 0; rep < reps; rep++) {
        glDispatchCompute(num_groups, 1, 1);
      }
      glFinish();
      glEndQuery(GL_TIME_ELAPSED);

      GLuint64 time2;
      glGetQueryObjectui64v(queries[1], GL_QUERY_RESULT, &time2);

      double seconds_per_rep = double(time2) / (1.0e9 * reps);
      double samp_per_sec = double(num_samples) / seconds_per_rep;

      log("usec/rep %f, gs/sec %f ", seconds_per_rep * 1.0e6, samp_per_sec / 1.0e9);
    }
  }

  //----------------------------------------
  // Run the merger

  {
    bind_compute_shader(merger_prog);
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, mip1_ssbo, 0, mip1_size_bytes);
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 1, mip2_ssbo, 0, mip2_size_bytes);

    int work_group_size[3];
    glGetProgramiv(mipper_prog, GL_COMPUTE_WORK_GROUP_SIZE, work_group_size);

    size_t num_groups = (mip1_size_bytes / 128) / work_group_size[0];
    if (num_groups == 0) num_groups = 1;

    log("Merge %ld threads", num_groups * work_group_size[0]);

    glFinish();
    glBeginQuery(GL_TIME_ELAPSED, queries[2]);

    size_t reps = 1000;
    for (size_t rep = 0; rep < 1000; rep++) {
      glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
      glDispatchCompute(num_groups, 1, 1);
    }
    glFinish();
    glEndQuery(GL_TIME_ELAPSED);

    GLuint64 time3;
    glGetQueryObjectui64v(queries[2], GL_QUERY_RESULT, &time3);

    log("Merge time %f usec", double(time3) * 1.0e-3 / reps);
  }

  //----------------------------------------

  glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 0, 0, 0);
  glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 1, 0, 0, 0);

  log("mip0");
  dump_ssbo(mip0_ssbo, mip0_size_bytes);

  log("mip1");
  dump_ssbo(mip1_ssbo, mip1_size_bytes);

  log("mip2");
  dump_ssbo(mip2_ssbo, mip2_size_bytes);


  log("TraceMipper::run() done");
}
