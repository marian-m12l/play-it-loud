#pragma once

#include <stdint.h>

#define PIN_SERIAL_CLOCK 0
#define PIN_SERIAL_MISO 1
#define PIN_SERIAL_MOSI 2

void gb_serial_init();
void gb_serial_start();
int gb_serial_needs_samples();
void gb_serial_fill_buffer(int16_t* samples);
void gb_serial_clear_buffers();
