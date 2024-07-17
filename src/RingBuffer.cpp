#include "RingBuffer.hpp"

RingBuffer::RingBuffer(size_t len) {

  cursor_read  = 0;
  cursor_ready = 0;
  cursor_write = 0;

  buffer = new uint8_t[len];
  buffer_len = len;
}

RingBuffer::~RingBuffer() {
  delete [] buffer;
  buffer = nullptr;
}
