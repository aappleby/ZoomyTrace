#pragma once
#include "symlinks/imgui/imgui.h"
#include <time.h>
#include <stdint.h>
#include <math.h>

//------------------------------------------------------------------------------

double timestamp();
void log(const char* format, ...);
void err(const char* format, ...);

void log_draw_imgui(const char* title);

uint32_t rng();

inline int64_t round_nearest(double x) {
  return (int64_t)floor(x + 0.5);
}

inline double remap(double x, double a1, double a2, double b1, double b2) {
  x -= a1;
  x /= (a2 - a1);
  x *= (b2 - b1);
  x += b1;
  return x;
}

//----------
// Crappy cubic sin(2*pi*x) approximation

inline double my_fsin(double x) {
  double y = abs(x - floor(x - 0.25) - 0.75) - 0.25;
  return y*(6.1922647442358311664092063140774168 - 35.3637069400432002923807340734237452*y*y);
}
