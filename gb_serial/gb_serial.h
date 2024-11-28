#pragma once

#include <stdint.h>

#define PIN_SERIAL_CLOCK 0
#define PIN_SERIAL_MISO 1
#define PIN_SERIAL_MOSI 2

void gb_serial_init();

void gb_serial_streaming_start();
void gb_serial_streaming_stop();
int gb_serial_streaming_needs_samples();
void gb_serial_streaming_fill_buffer(int16_t* samples);
void gb_serial_streaming_clear_buffers();

void gb_serial_immediate_transfer(uint8_t* data, int length);

bool gb_serial_transfer_done();
