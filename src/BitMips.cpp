#include "BitMips.hpp"

#include "log.hpp"
#include <stdio.h>
#include <assert.h>

//------------------------------------------------------------------------------

BitMips::BitMips() {
  sample_count = 0;
  mip1_len = 0;
  mip2_len = 0;
  mip3_len = 0;
  mip4_len = 0;

  size_t max_sample_count = (1 << 28);

  size_t max_mip1_len = (max_sample_count + 127) / 128;
  mip1 = new uint8_t[max_mip1_len];

  size_t max_mip2_len = (max_mip1_len + 127) / 128;
  mip2 = new uint8_t[max_mip2_len];

  size_t max_mip3_len = (max_mip2_len + 127) / 128;
  mip3 = new uint8_t[max_mip3_len];

  size_t max_mip4_len = (max_mip3_len + 127) / 128;
  mip4 = new uint8_t[max_mip4_len];
}

BitMips::~BitMips() {
  //delete [] bits;
  delete [] mip1;
  delete [] mip2;
  delete [] mip3;
  delete [] mip4;
}

//------------------------------------------------------------------------------

void BitMips::update_mips(BitBlob& blob, int channel) {

  auto bits = blob.bits;
  this->sample_count = blob.sample_count;
  size_t bits_len = sample_count / 8;

  // round totals *up* so that we don't mark a sparse mip as completely empty.
  mip1_len = (sample_count + 127) / 128;
  mip2_len = (mip1_len + 127) / 128;
  mip3_len = (mip2_len + 127) / 128;
  mip4_len = (mip3_len + 127) / 128;

  //----------
  // update mip1

  {
    size_t cursor = 0;
    for (size_t i = 0; i < mip1_len; i++) {

      int total = 0;
      for (size_t j = 0; j < 128; j++) {
        total += blob.get_bit(channel, i * 128 + j);
        if (cursor == sample_count) break;
      }

      mip1[i] = total;
      if (cursor == sample_count) break;
    }
  }

  //----------
  // update mip2

  {
    size_t cursor = 0;
    for (size_t i = 0; i < mip2_len; i++) {
      int total = 0;
      for (size_t j = 0; j < 128; j++) {
        total += mip1[cursor++];
        if (cursor == mip1_len) break;
      }
      mip2[i] = (total + 127) >> 7;
      if (cursor == mip1_len) break;
    }
  }

  //----------
  // update mip3

  {
    size_t cursor = 0;
    for (size_t i = 0; i < mip3_len; i++) {
      int total = 0;
      for (size_t j = 0; j < 128; j++) {
        total += mip2[cursor++];
        if (cursor == mip2_len) break;
      }
      mip3[i] = (total + 127) >> 7;
      if (cursor == mip2_len) break;
    }
  }

  //----------
  // update mip4

  {
    size_t cursor = 0;
    for (size_t i = 0; i < mip4_len; i++) {
      int total = 0;
      for (int j = 0; j < 128; j++) {
        total += mip3[cursor++];
        if (cursor == mip3_len) break;
      }
      mip4[i] = (total + 127) >> 7;
      if (cursor == mip3_len) break;
    }
  }
}


//------------------------------------------------------------------------------

//#pragma GCC optimize("O0")
void BitMips::render(BitBlob& blob, int channel, double bar_min, double bar_max, double view_min, double view_max, double* filtered, int window_width) {

  uint8_t* bits = blob.bits;

  const uint64_t mip0_shift =  0;
  const uint64_t mip1_shift =  7;
  const uint64_t mip2_shift = 14;
  const uint64_t mip3_shift = 21;
  const uint64_t mip4_shift = 28;

  const uint64_t mip0_size = 0b00000000000000000000000000000000001ull;
  const uint64_t mip1_size = 0b00000000000000000000000000010000000ull;
  const uint64_t mip2_size = 0b00000000000000000000100000000000000ull;
  const uint64_t mip3_size = 0b00000000000001000000000000000000000ull;
  const uint64_t mip4_size = 0b00000010000000000000000000000000000ull;

  const uint64_t mip0_mask = 0b00000000000000000000000000001111111ull;
  const uint64_t mip1_mask = 0b00000000000000000000011111110000000ull;
  const uint64_t mip2_mask = 0b00000000000000111111100000000000000ull;
  const uint64_t mip3_mask = 0b00000001111111000000000000000000000ull;
  const uint64_t mip4_mask = 0b11111110000000000000000000000000000ull;

  const uint64_t mip0_weight = 128;
  const uint64_t mip1_weight = 128;
  const uint64_t mip2_weight = 128 * 128;
  const uint64_t mip3_weight = 128 * 128 * 128;
  const uint64_t mip4_weight = 128 * 128 * 128 * 128;

  // We compute a "granularity" based on the width of each pixel in trace-space
  // so that we can reduce the precision of the span endpoints and skip lower
  // mips when they wouldn't contribute to the final value much. 7 here means
  // 1/128-subpixel precision.
  double pix0_l = remap(0.0, bar_min, bar_max, view_min, view_max);
  double pix0_r = remap(1.0, bar_min, bar_max, view_min, view_max);
  double granularity = exp2(ceil(log2(pix0_r - pix0_l)) - 7);
  double igranularity = 1.0 / granularity;

  uint32_t* dwords = (uint32_t*)bits;

  int mip0_hit = 0;
  int mip1_hit = 0;
  int mip2_hit = 0;
  int mip3_hit = 0;
  int mip4_hit = 0;

  for (int x = 0; x < window_width; x++) {
    filtered[x] = 0;

    double pixel_center = x + 0.5;

    double sample_fmin = remap(pixel_center - 0.5, bar_min, bar_max, view_min, view_max);
    double sample_fmax = remap(pixel_center + 0.5, bar_min, bar_max, view_min, view_max);

    // If the span is really large, ditch some precision from the end bits so
    // we don't bother with head/tail samples that don't contribute to the sum
    sample_fmin = floor(sample_fmin * igranularity) * granularity;
    sample_fmax = floor(sample_fmax * igranularity) * granularity;

    if (sample_fmin < 0)            sample_fmin = 0;
    if (sample_fmax > sample_count) sample_fmax = sample_count;

    if (sample_fmax < 0)             { filtered[x] = 0; continue; }
    if (sample_fmin >= sample_count) { filtered[x] = 0; continue; }

    //----------------------------------------
    // Samples coords start as 32.7 fixed point, 7 bit subpixel precision

    int64_t sample_imin = (int64_t)floor(sample_fmin * 128.0);
    int64_t sample_imax = (int64_t)floor(sample_fmax * 128.0);

    // Both endpoints are inside the same sample.
    if ((sample_imin >> 7) == (sample_imax >> 7)) {
      filtered[x] = blob.get_bit(channel, sample_imin >> 7);
      mip0_hit++;
      continue;
    }

    uint64_t sample_ilen = sample_imax - sample_imin;
    uint64_t total = 0;

    // Add the contribution from the fractional portion of the first sample.
    if (sample_imin & 0x7F) {
      double head_fract = 128 - (sample_imin & 0x7F);
      total += head_fract * blob.get_bit(channel, sample_imin >> 7);
      sample_imin = (sample_imin + 0x7F) & ~0x7F;
      mip0_hit++;
    }

    // Add the contribution from the fractional portion of the last sample.
    if (sample_imax & 0x7F) {
      double tail_fract = sample_imax & 0x7F;
      total += tail_fract * blob.get_bit(channel, sample_imax >> 7);
      sample_imax = sample_imax & ~0x7F;
      mip0_hit++;
    }

    if (sample_imin == sample_imax) {
      filtered[x] = double(total) / double(sample_ilen);
      continue;
    }

    assert(!(sample_imin & 0x7F));
    assert(!(sample_imax & 0x7F));
    sample_imin >>= 7;
    sample_imax >>= 7;

    //----------------------------------------
    // Endpoints are now integers. This could be slightly faster with popcnt,
    // but it's not really worth it.

    if ((sample_imin & mip0_mask) || (sample_imax & mip0_mask)) {
      while ((sample_imin & mip0_mask) && (sample_imin < sample_imax)) {
        total += blob.get_bit(channel, sample_imin >> mip0_shift) * mip0_weight;
        sample_imin += mip0_size;
        mip0_hit++;
      }

      while ((sample_imax & mip0_mask) && (sample_imin < sample_imax)) {
        total += blob.get_bit(channel, (sample_imax - 1) >> mip0_shift) * mip0_weight;
        sample_imax -= mip0_size;
        mip0_hit++;
      }

      if (sample_imin == sample_imax) {
        filtered[x] = double(total) / double(sample_ilen);
        continue;
      }

      assert(!(sample_imin & mip0_mask));
      assert(!(sample_imax & mip0_mask));
    }

    //----------------------------------------
    // Endpoints are now multiples of mip1_size

    if ((sample_imin & mip1_mask) || (sample_imax & mip1_mask)) {
      while ((sample_imin & mip1_mask) && (sample_imin < sample_imax)) {
        total += mip1[sample_imin >> mip1_shift] * mip1_weight;
        sample_imin += mip1_size;
        mip1_hit++;
      }

      while ((sample_imax & mip1_mask) && (sample_imin < sample_imax)) {
        total += mip1[(sample_imax - 1) >> mip1_shift] * mip1_weight;
        sample_imax -= mip1_size;
        mip1_hit++;
      }

      if (sample_imin == sample_imax) {
        filtered[x] = double(total) / double(sample_ilen);
        continue;
      }

      assert(!(sample_imin & mip1_mask));
      assert(!(sample_imax & mip1_mask));
    }

    //----------------------------------------
    // Endpoints are now multiples of mip2_size

    if ((sample_imin & mip2_mask) || (sample_imax & mip2_mask)) {
      while ((sample_imin & mip2_mask) && (sample_imin < sample_imax)) {
        total += mip2[sample_imin >> mip2_shift] * mip2_weight;
        sample_imin += mip2_size;
        mip2_hit++;
      }

      while ((sample_imax & mip2_mask) && (sample_imin < sample_imax)) {
        total += mip2[(sample_imax - 1) >> mip2_shift] * mip2_weight;
        sample_imax -= mip2_size;
        mip2_hit++;
      }

      if (sample_imin == sample_imax) {
        filtered[x] = double(total) / double(sample_ilen);
        continue;
      }

      assert(!(sample_imin & mip2_mask));
      assert(!(sample_imax & mip2_mask));
    }

    //----------------------------------------
    // Endpoints are now multiples of mip3_size

    if ((sample_imin & mip3_mask) || (sample_imax & mip3_mask)) {
      while ((sample_imin & mip3_mask) && (sample_imin < sample_imax)) {
        total += mip3[sample_imin >> mip3_shift] * mip3_weight;
        sample_imin += mip3_size;
        mip3_hit++;
      }

      while ((sample_imax & mip3_mask) && (sample_imin < sample_imax)) {
        total += mip3[(sample_imax - 1) >> mip3_shift] * mip3_weight;
        sample_imax -= mip3_size;
        mip3_hit++;
      }

      if (sample_imin == sample_imax) {
        filtered[x] = double(total) / double(sample_ilen);
        continue;
      }

      assert(!(sample_imin & mip3_mask));
      assert(!(sample_imax & mip3_mask));
    }

    //----------------------------------------
    // Endpoints are now multiples of mip4_size

    while (sample_imin < sample_imax) {
      total += mip4[sample_imin >> mip4_shift] * mip4_weight;
      sample_imin += mip4_size;
      mip4_hit++;
    }

    //----------------------------------------
    // Done

    filtered[x] = double(total) / double(sample_ilen);
  }

  //printf("mip0_hit %d\n", mip0_hit);
  //printf("mip1_hit %d\n", mip1_hit);
  //printf("mip2_hit %d\n", mip2_hit);
  //printf("mip3_hit %d\n", mip3_hit);
  //printf("mip4_hit %d\n", mip4_hit);
}

//------------------------------------------------------------------------------

void BitMips::dump() {
  assert(!(sample_count & 127));

  //printf("samples  %ld\n", sample_count);
  //printf("bits_len %ld\n", bits_len);
  printf("mip1_len %ld\n", mip1_len);
  printf("mip2_len %ld\n", mip2_len);
  printf("mip3_len %ld\n", mip3_len);
  printf("mip4_len %ld\n", mip4_len);
  printf("\n");

  {
    printf("mip1:\n");
    size_t rows = (mip1_len + 63) / 64;
    size_t cols = 64;
    size_t cursor = 0;

    for (size_t y = 0; y < rows; y++) {
      for (size_t x = 0; x < cols; x++) {
        printf("%02x ", mip1[cursor++]);
        if (cursor == mip1_len) break;
      }
      putc('\n', stdout);
      if (cursor == mip1_len) break;
    }
    printf("\n");
  }

  {
    printf("mip2:\n");
    size_t rows = (mip2_len + 63) / 64;
    size_t cols = 64;
    size_t cursor = 0;

    for (size_t y = 0; y < rows; y++) {
      for (size_t x = 0; x < cols; x++) {
        printf("%02x ", mip2[cursor++]);
        if (cursor == mip2_len) break;
      }
      putc('\n', stdout);
      if (cursor == mip2_len) break;
    }
    printf("\n");
  }

  {
    printf("mip3:\n");
    size_t rows = (mip3_len + 63) / 64;
    size_t cols = 64;
    size_t cursor = 0;

    for (size_t y = 0; y < rows; y++) {
      for (size_t x = 0; x < cols; x++) {
        printf("%02x ", mip3[cursor++]);
        if (cursor == mip3_len) break;
      }
      putc('\n', stdout);
      if (cursor == mip3_len) break;
    }
    printf("\n");
  }

  {
    printf("mip4:\n");
    size_t rows = (mip4_len + 63) / 64;
    size_t cols = 64;
    size_t cursor = 0;

    for (size_t y = 0; y < rows; y++) {
      for (size_t x = 0; x < cols; x++) {
        printf("%02x ", mip4[cursor++]);
        if (cursor == mip4_len) break;
      }
      putc('\n', stdout);
      if (cursor == mip4_len) break;
    }
    printf("\n");
  }
}

//------------------------------------------------------------------------------
