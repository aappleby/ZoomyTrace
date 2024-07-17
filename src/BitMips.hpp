#pragma once
#include <stdint.h>
#include <stddef.h>
#include "log.hpp"
#include <stdio.h>
#include <assert.h>
#include "BitBlob.hpp"

struct BitMips {
  BitMips();
  ~BitMips();

  void update_mips(BitBlob& blob, int channel);
  void render(
    BitBlob& bits, int channel,
    double bar_min, double bar_max,
    double view_min, double view_max,
    double* filtered, int window_width
  );
  void dump();

  size_t sample_count;

  size_t mip1_len;
  size_t mip2_len;
  size_t mip3_len;
  size_t mip4_len;

  uint8_t*  mip1 = nullptr;
  uint8_t*  mip2 = nullptr;
  uint8_t*  mip3 = nullptr;
  uint8_t*  mip4 = nullptr;
};
