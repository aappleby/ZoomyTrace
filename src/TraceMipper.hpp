#pragma once
#include <stddef.h>
#include <stdint.h>

struct TraceMipper {

  void init();
  void exit();
  void run(int trace_ssbo, int mip_ssbo);

  uint32_t mipper_prog;
  uint32_t mipper_ubo;

  size_t num_samples;
  size_t num_channels;

  size_t mip0_size_bytes;
  size_t mip1_size_bytes;

  uint32_t mip0_ssbo;
  uint32_t mip1_ssbo;

  uint32_t queries[32];
};
