#pragma once
#include <stdint.h>
#include <atomic>

extern uint8_t* ring_buffer;
extern std::atomic_int cursor_write;
extern std::atomic_int cursor_read;

int capture();
