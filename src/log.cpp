#include "log.hpp"
#include <stdio.h>
#include <string>

static char temp_buf[256];
std::string log_buf;

//------------------------------------------------------------------------------

double timestamp() {
  static uint64_t _time_origin = 0;
  timespec ts;
  (void)timespec_get(&ts, TIME_UTC);
  uint64_t now = ts.tv_sec * 1000000000ull + ts.tv_nsec;
  if (!_time_origin) _time_origin = now;
  return double(now - _time_origin) / 1.0e9;
}

//------------------------------------------------------------------------------

void log_color(uint32_t color, const char* format, va_list args) {
  va_list args2;
  va_copy(args2, args);

  printf("\u001b[38;2;%d;%d;%dm", (color >> 0) & 0xFF, (color >> 8) & 0xFF, (color >> 16) & 0xFF);
  printf("[%010.6f] ", timestamp());
  vprintf(format, args);
  printf("\u001b[0m\n");

  snprintf(temp_buf, sizeof(temp_buf), "[%010.6f] ", timestamp());
  log_buf += temp_buf;
  vsnprintf(temp_buf, sizeof(temp_buf), format, args2);
  log_buf += temp_buf;
  log_buf += "\n";
  fflush(stdout);
}

//------------------------------------------------------------------------------

void log(const char* format, ...) {
  va_list args;
  va_start(args, format);
  log_color(0x00CCCCCC, format, args);
}

void log_r(const char* format, ...) {
  va_list args;
  va_start(args, format);
  log_color(0x008888CC, format, args);
}

void log_g(const char* format, ...) {
  va_list args;
  va_start(args, format);
  log_color(0x0088CC88, format, args);
}

void log_b(const char* format, ...) {
  va_list args;
  va_start(args, format);
  log_color(0x00CC8888, format, args);
}

void err(const char* format, ...) {
  va_list args;
  va_start(args, format);
  log_color(0x008888FF, format, args);
}

//------------------------------------------------------------------------------

uint32_t rng() {
  static uint32_t a = 1;
  a *= 0x12375457;
  a ^= a << 13;
  a ^= a >> 17;
  a ^= a << 5;
  return a;
}

//------------------------------------------------------------------------------
