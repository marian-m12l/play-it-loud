#pragma once

#include <stdint.h>

#define PIN_SERIAL_CLOCK 0
#define PIN_SERIAL_MISO 1
#define PIN_SERIAL_MOSI 2

typedef struct {
    uint16_t buffer_size;
    void (*swap)(void);
    uint8_t* (*playback_buffer)(void);
} gb_serial_source_t;

void gb_serial_init(uint16_t transfer_rate);
void gb_serial_set_transfer_rate(uint16_t transfer_rate);

void gb_serial_streaming_start(gb_serial_source_t* source);
void gb_serial_streaming_stop();
bool gb_serial_streaming_is_busy();

void gb_serial_immediate_transfer(const uint8_t* data, uint16_t length);

bool gb_serial_transfer_done();
uint8_t gb_serial_received();
