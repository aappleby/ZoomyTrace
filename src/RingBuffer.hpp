#pragma once
#include <atomic>
#include <stdint.h>

struct RingBuffer {

  RingBuffer(size_t len);
  ~RingBuffer();

  // DMA ring buffer size seems to be limited to 8 megs (128 chunks)
  //int chunk_size  = 65536;
  //int chunk_count = 128;
  //int ring_size   = 65536 * 128;
  //int ring_mask   = (65536 * 128) - 1;

  std::atomic_int cursor_read  = 0;
  std::atomic_int cursor_ready = 0;
  std::atomic_int cursor_write = 0;

  uint8_t* buffer = nullptr;
  size_t   buffer_len = 0;

};
