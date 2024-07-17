#pragma once
#include <stdint.h>
#include <stddef.h>
#include <assert.h>

struct BitBlob {

  /*
  BitBlob(size_t sample_count, size_t channels, size_t stride) {
    this->channels = 8;
    this->sample_count = 1024ull * 1024ull;
    this->stride = 8;

    size_t max_index = sample_count * stride;
    this->bits_len = (max_index + 7) / 8;
    this->bits = new uint8_t[this->bits_len];
  }
  */

  int get_bit(size_t channel, size_t sample) {
    assert(channel < channels);
    size_t index = channel + (sample * stride);
    assert((index / 8) < bits_len);
    return (bits[index >> 3] >> (index & 7)) & 1;
  }

  size_t   channels;
  size_t   sample_count;
  size_t   stride;
  size_t   bits_len;
  uint8_t* bits;
};
