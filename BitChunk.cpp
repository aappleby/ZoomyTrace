#include "BitChunk.hpp"

#include "log.hpp"
#include <stdio.h>
#include <assert.h>

//------------------------------------------------------------------------------

void gen_pattern(void* buf, size_t sample_count) {

  printf("generating pattern\n");

  auto time_a = timestamp();

  uint32_t* bits = (uint32_t*)buf;

  uint32_t x = 1;
  for (size_t i = 0; i < sample_count; i++) {
    size_t t = i;
    t = t*((t>>9|t>>13)&25&t>>6);
    //t *= 0x123;
    //t ^= t >> 27;
    t = __builtin_parity(t);
    bits[(i >> 5)] |= (t << (i & 31));
  }

  /*
  for (size_t i = 0; i < sample_count; i++) {
    double t = double(i) / double(sample_count);

    t = my_fsin(t * 100) * t * 0.5 + 0.5;
    t *= double(0xFFFFFFFF);
    int s = int(rng() <= t);

    //if ((i % 17327797) < 4537841) {
    //  s = 0;
    //}

    bits[(i >> 5)] |= (s << (i & 31));
  }
  */

  printf("generating pattern done in %12.8f sec\n", timestamp() - time_a);
}

//----------




//#pragma GCC optimize("O0")
void BitBuf::update_table(double bar_min, double bar_max, double view_min, double view_max, double* filtered, int window_width) {

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
      filtered[x] = get_bit(bits, sample_imin >> 7);
      mip0_hit++;
      continue;
    }

    uint64_t sample_ilen = sample_imax - sample_imin;
    uint64_t total = 0;

    // Add the contribution from the fractional portion of the first sample.
    if (sample_imin & 0x7F) {
      double head_fract = 128 - (sample_imin & 0x7F);
      total += head_fract * get_bit(bits, sample_imin >> 7);
      sample_imin = (sample_imin + 0x7F) & ~0x7F;
      mip0_hit++;
    }

    // Add the contribution from the fractional portion of the last sample.
    if (sample_imax & 0x7F) {
      double tail_fract = sample_imax & 0x7F;
      total += tail_fract * get_bit(bits, sample_imax >> 7);
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
        total += get_bit(bits, sample_imin >> mip0_shift) * mip0_weight;
        sample_imin += mip0_size;
        mip0_hit++;
      }

      while ((sample_imax & mip0_mask) && (sample_imin < sample_imax)) {
        total += get_bit(bits, (sample_imax - 1) >> mip0_shift) * mip0_weight;
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

  printf("mip0_hit %d\n", mip0_hit);
  printf("mip1_hit %d\n", mip1_hit);
  printf("mip2_hit %d\n", mip2_hit);
  printf("mip3_hit %d\n", mip3_hit);
  printf("mip4_hit %d\n", mip4_hit);
}
