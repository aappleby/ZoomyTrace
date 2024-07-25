#include "Bits.hpp"

#include "log.hpp"

//------------------------------------------------------------------------------

void generate_mip1(void* blob, size_t samples, size_t channels, size_t stride, int mip_channel, uint8_t* out) {
  uint32_t* words = (uint32_t*)blob;

  for (size_t i = 0; i < samples / 128; i++) {
    int total = 0;
    for (size_t j = 0; j < 128; j++) {
      size_t index = (i * 128 + j) * stride + mip_channel;
      total += (words[index >> 5] >> (index & 31)) & 1;
    }
    out[i] = total;
  }
}

//------------------------------------------------------------------------------

void update_mips(TraceBuffer& trace, int channel, size_t sample_min, size_t sample_max, MipBuffer& mips) {
  auto mip1_min = (sample_min +   0) >> 7;
  auto mip1_max = (sample_max + 127) >> 7;

  for (size_t i = mip1_min; i < mip1_max; i++) {
    int total = 0;
    for (size_t j = 0; j < 128; j++) {
      auto index = i * 128 + j;
      if (index >= trace.samples) break;
      total += trace.get_bit(channel, index);
    }
    mips.mip1[i] = total;
  }

  auto mip2_min = (mip1_min +   0) >> 7;
  auto mip2_max = (mip1_max + 127) >> 7;

  for (size_t i = mip2_min; i < mip2_max; i++) {
    int total = 0;
    for (size_t j = 0; j < 128; j++) {
      auto index = i * 128 + j;
      if (index >= mips.mip1_len) break;
      total += mips.mip1[index];
    }
    mips.mip2[i] = (total + 127) >> 7;
  }

  auto mip3_min = (mip2_min +   0) >> 7;
  auto mip3_max = (mip2_max + 127) >> 7;

  for (size_t i = mip3_min; i < mip3_max; i++) {
    int total = 0;
    for (size_t j = 0; j < 128; j++) {
      auto index = i * 128 + j;
      if (index >= mips.mip2_len) break;
      total += mips.mip2[i * 128 + j];
    }
    mips.mip3[i] = (total + 127) >> 7;
  }

  auto mip4_min = (mip3_min +   0) >> 7;
  auto mip4_max = (mip3_max + 127) >> 7;

  for (size_t i = mip4_min; i < mip4_max; i++) {
    int total = 0;
    for (int j = 0; j < 128; j++) {
      auto index = i * 128 + j;
      if (index >= mips.mip3_len) break;
      total += mips.mip3[i * 128 + j];
    }
    mips.mip4[i] = (total + 127) >> 7;
  }
}

//------------------------------------------------------------------------------

//#pragma GCC optimize("O0")
void render(
  TraceBuffer& trace, MipBuffer& mips, int channel,
  double world_min, double world_max,
  double trace_min, double trace_max,
  double* out, int out_len)
{
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

  // We compute a "granularity" based on the width of each pixel in trace space
  // so that we can reduce the precision of the span endpoints and skip lower
  // mips when they wouldn't contribute to the final value much. 7 here means
  // 1/128-subpixel precision.

  // FIXME - use samples_per_pixel or something
  double pix0_l = remap(0.0, world_min, world_max, trace_min, trace_max);
  double pix0_r = remap(1.0, world_min, world_max, trace_min, trace_max);
  double granularity = exp2(ceil(log2(pix0_r - pix0_l)) - 7);
  double igranularity = 1.0 / granularity;

  mips.mip0_hit = 0;
  mips.mip1_hit = 0;
  mips.mip2_hit = 0;
  mips.mip3_hit = 0;
  mips.mip4_hit = 0;

  for (int x = 0; x < out_len; x++) {
    out[x] = 0;

    double pixel_center = x + 0.5;

    double sample_fmin = remap(pixel_center - 0.5, world_min, world_max, trace_min, trace_max);
    double sample_fmax = remap(pixel_center + 0.5, world_min, world_max, trace_min, trace_max);

    // If the span is really large, ditch some precision from the end bits so
    // we don't bother with head/tail samples that don't contribute to the sum
    sample_fmin = floor(sample_fmin * igranularity) * granularity;
    sample_fmax = floor(sample_fmax * igranularity) * granularity;

    if (sample_fmin < 0)             sample_fmin = 0;
    if (sample_fmax > trace.samples) sample_fmax = trace.samples;

    if (sample_fmax < 0)              { out[x] = 0; continue; }
    if (sample_fmin >= trace.samples) { out[x] = 0; continue; }

    //----------------------------------------
    // Samples coords start as 32.7 fixed point, 7 bit subpixel precision

    int64_t sample_imin = (int64_t)floor(sample_fmin * 128.0);
    int64_t sample_imax = (int64_t)floor(sample_fmax * 128.0);

    // Both endpoints are inside the same sample.
    if ((sample_imin >> 7) == (sample_imax >> 7)) {
      out[x] = trace.get_bit(channel, sample_imin >> 7);
      mips.mip0_hit++;
      continue;
    }

    uint64_t sample_ilen = sample_imax - sample_imin;
    uint64_t total = 0;

    // Add the contribution from the fractional portion of the first sample.
    if (sample_imin & 0x7F) {
      double head_fract = 128 - (sample_imin & 0x7F);
      total += head_fract * trace.get_bit(channel, sample_imin >> 7);
      sample_imin = (sample_imin + 0x7F) & ~0x7F;
      mips.mip0_hit++;
    }

    // Add the contribution from the fractional portion of the last sample.
    if (sample_imax & 0x7F) {
      double tail_fract = sample_imax & 0x7F;
      total += tail_fract * trace.get_bit(channel, sample_imax >> 7);
      sample_imax = sample_imax & ~0x7F;
      mips.mip0_hit++;
    }

    if (sample_imin == sample_imax) {
      out[x] = double(total) / double(sample_ilen);
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
        total += trace.get_bit(channel, sample_imin >> mip0_shift) * mip0_weight;
        sample_imin += mip0_size;
        mips.mip0_hit++;
      }

      while ((sample_imax & mip0_mask) && (sample_imin < sample_imax)) {
        total += trace.get_bit(channel, (sample_imax - 1) >> mip0_shift) * mip0_weight;
        sample_imax -= mip0_size;
        mips.mip0_hit++;
      }

      if (sample_imin == sample_imax) {
        out[x] = double(total) / double(sample_ilen);
        continue;
      }

      assert(!(sample_imin & mip0_mask));
      assert(!(sample_imax & mip0_mask));
    }

    //----------------------------------------
    // Endpoints are now multiples of mip1_size

    if ((sample_imin & mip1_mask) || (sample_imax & mip1_mask)) {
      while ((sample_imin & mip1_mask) && (sample_imin < sample_imax)) {
        total += mips.mip1[sample_imin >> mip1_shift] * mip1_weight;
        sample_imin += mip1_size;
        mips.mip1_hit++;
      }

      while ((sample_imax & mip1_mask) && (sample_imin < sample_imax)) {
        total += mips.mip1[(sample_imax - 1) >> mip1_shift] * mip1_weight;
        sample_imax -= mip1_size;
        mips.mip1_hit++;
      }

      if (sample_imin == sample_imax) {
        out[x] = double(total) / double(sample_ilen);
        continue;
      }

      assert(!(sample_imin & mip1_mask));
      assert(!(sample_imax & mip1_mask));
    }

    //----------------------------------------
    // Endpoints are now multiples of mip2_size

    if ((sample_imin & mip2_mask) || (sample_imax & mip2_mask)) {
      while ((sample_imin & mip2_mask) && (sample_imin < sample_imax)) {
        total += mips.mip2[sample_imin >> mip2_shift] * mip2_weight;
        sample_imin += mip2_size;
        mips.mip2_hit++;
      }

      while ((sample_imax & mip2_mask) && (sample_imin < sample_imax)) {
        total += mips.mip2[(sample_imax - 1) >> mip2_shift] * mip2_weight;
        sample_imax -= mip2_size;
        mips.mip2_hit++;
      }

      if (sample_imin == sample_imax) {
        out[x] = double(total) / double(sample_ilen);
        continue;
      }

      assert(!(sample_imin & mip2_mask));
      assert(!(sample_imax & mip2_mask));
    }

    //----------------------------------------
    // Endpoints are now multiples of mip3_size

    if ((sample_imin & mip3_mask) || (sample_imax & mip3_mask)) {
      while ((sample_imin & mip3_mask) && (sample_imin < sample_imax)) {
        total += mips.mip3[sample_imin >> mip3_shift] * mip3_weight;
        sample_imin += mip3_size;
        mips.mip3_hit++;
      }

      while ((sample_imax & mip3_mask) && (sample_imin < sample_imax)) {
        total += mips.mip3[(sample_imax - 1) >> mip3_shift] * mip3_weight;
        sample_imax -= mip3_size;
        mips.mip3_hit++;
      }

      if (sample_imin == sample_imax) {
        out[x] = double(total) / double(sample_ilen);
        continue;
      }

      assert(!(sample_imin & mip3_mask));
      assert(!(sample_imax & mip3_mask));
    }

    //----------------------------------------
    // Endpoints are now multiples of mip4_size

    while (sample_imin < sample_imax) {
      total += mips.mip4[sample_imin >> mip4_shift] * mip4_weight;
      sample_imin += mip4_size;
      mips.mip4_hit++;
    }

    //----------------------------------------
    // Done

    out[x] = double(total) / double(sample_ilen);
  }
}

//------------------------------------------------------------------------------

#if 0
void BitMips::dump() {
  assert(!(sample_count & 127));

  printf("samples  %ld\n", sample_count);
  //printf("mip1_len %ld\n", mip1_len);
  //printf("mip2_len %ld\n", mip2_len);
  //printf("mip3_len %ld\n", mip3_len);
  //printf("mip4_len %ld\n", mip4_len);

  size_t mip1_len = (sample_count + 127) / 128;
  size_t mip2_len = (mip1_len + 127) / 128;
  size_t mip3_len = (mip2_len + 127) / 128;
  size_t mip4_len = (mip3_len + 127) / 128;

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
#endif

//------------------------------------------------------------------------------
