#pragma once
#include <stdint.h>
#include <stddef.h>
#include "log.hpp"
#include <stdio.h>
#include <assert.h>

int get_bit(void* buf, size_t sample_count, uint32_t i);


void gen_pattern(void* buf, size_t sample_count);

struct BitBuf {
  size_t sample_count;

  size_t bits_len;
  size_t mip1_len;
  size_t mip2_len;
  size_t mip3_len;
  size_t mip4_len;

  uint8_t*  bits = nullptr;
  uint8_t*  mip1 = nullptr;
  uint8_t*  mip2 = nullptr;
  uint8_t*  mip3 = nullptr;
  uint8_t*  mip4 = nullptr;

  ~BitBuf() {
    delete [] bits;
    delete [] mip1;
    delete [] mip2;
    delete [] mip3;
    delete [] mip4;
  }

  void update_table(double bar_min, double bar_max, double view_min, double view_max, double* filtered, int window_width);

  void init(size_t sample_count) {
    assert(!(sample_count & 127));
    this->sample_count = sample_count;

    bits_len = sample_count / 8;
    bits = new uint8_t[bits_len];

    mip1_len = (sample_count + 127) / 128;
    mip1 = new uint8_t[mip1_len];

    mip2_len = (mip1_len + 127) / 128;
    mip2 = new uint8_t[mip2_len];

    mip3_len = (mip2_len + 127) / 128;
    mip3 = new uint8_t[mip3_len];

    mip4_len = (mip3_len + 127) / 128;
    mip4 = new uint8_t[mip4_len];
  }

  //----------------------------------------

  void upload(void* buf, uint32_t a, uint32_t b) {
    assert(!(a & 31));
    assert(!(b & 31));
    memcpy(&bits[a/8], buf, (b-a)/8);
  }

  void update() {

    // round totals *up* so that we don't mark a sparse mip as completely empty.

    //----------
    // update mip1

    {
      size_t cursor = 0;
      for (size_t i = 0; i < mip1_len; i++) {

        int total = 0;
        for (size_t j = 0; j < 16; j++) {
          total += __builtin_popcount(bits[cursor++]);
          if (cursor == bits_len) break;
        }

        mip1[i] = total;
        if (cursor == bits_len) break;
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

  //----------------------------------------

  void dump() {
    assert(!(sample_count & 127));

    printf("samples  %ld\n", sample_count);
    printf("bits_len %ld\n", bits_len);
    printf("mip1_len %ld\n", mip1_len);
    printf("mip2_len %ld\n", mip2_len);
    printf("mip3_len %ld\n", mip3_len);
    printf("mip4_len %ld\n", mip4_len);
    printf("\n");

    {
      printf("bits:\n");
      size_t rows = (sample_count + 127) / 128;
      size_t cols = 128;
      size_t cursor = 0;

      for (size_t y = 0; y < rows; y++) {
        for (size_t x = 0; x < cols; x++) {
          putc(get_bit(bits, sample_count, cursor++) ? '1' : '0', stdout);
          if (cursor == sample_count) break;
        }
        putc('\n', stdout);
        if (cursor == sample_count) break;
      }
    }

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

  //----------------------------------------
};
