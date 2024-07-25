#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <assert.h>

// A buffer full of interleaved logic samples.
struct TraceBuffer {
  size_t samples = 0;
  size_t channels = 0;
  size_t stride = 0;   // Distance in bits between samples in the same channel.

  // GPU storage buffer
  int    ssbo = 0;
  size_t ssbo_len = 0;

  // Pointer into mapped GPU memory
  void*  blob = nullptr;

  int get_bit(size_t channel, size_t sample) {
    assert(channel < channels);
    if (sample >= samples) {
      printf("%ld %ld\n", sample, samples);
    }
    assert(sample < samples);
    assert(((sample * stride + channel) / 8) < ssbo_len);

    uint32_t* words = (uint32_t*)blob;
    size_t index = sample * stride + channel;
    return (words[index >> 5] >> (index & 31)) & 1;
  }
};

// Mipmaps for one channel of logic trace
struct MipBuffer {
  size_t samples;

  // GPU storage buffer
  int    ssbo = 0;
  size_t ssbo_len = 0;

  // Pointers into mapped GPU memory
  uint8_t* mip1 = nullptr;
  uint8_t* mip2 = nullptr;
  uint8_t* mip3 = nullptr;
  uint8_t* mip4 = nullptr;

  size_t mip1_len;
  size_t mip2_len;
  size_t mip3_len;
  size_t mip4_len;

  size_t mip1_offset;
  size_t mip2_offset;
  size_t mip3_offset;
  size_t mip4_offset;

  size_t mip0_hit = 0;
  size_t mip1_hit = 0;
  size_t mip2_hit = 0;
  size_t mip3_hit = 0;
  size_t mip4_hit = 0;

  //uint32_t mip4[1];
  //uint32_t mip3[128];
  //uint32_t mip2[16384];
  //uint32_t mip1[2097152];
};

void update_mips(TraceBuffer& trace, int channel, size_t sample_min, size_t sample_max, MipBuffer& mips);

void render(TraceBuffer& trace, MipBuffer& mips, int channel,
            double world_min, double world_max,
            double trace_min, double trace_max,
            double* out, int out_len);
