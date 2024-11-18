#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"

#include "gb_serial.h"

// Declare pins in binary info
bi_decl(bi_3pins_with_names(
    PIN_SERIAL_CLOCK, "GB Serial Clock",
    PIN_SERIAL_MISO, "GB Serial IN",
    PIN_SERIAL_MOSI, "GB Serial OUT"
));


#define SINE_WAVE_TABLE_LEN 2048
static int16_t sine_wave_table[SINE_WAVE_TABLE_LEN];
#define POLL_RATE_MS 5

int main() {
    stdio_init_all();

    printf("Sine Wave Player\n");

    // Prepare sine samples
    for (int i = 0; i < SINE_WAVE_TABLE_LEN; i++) {
        sine_wave_table[i] = 32767 * cosf(i * 2 * (float) (M_PI / SINE_WAVE_TABLE_LEN));
    }
    uint32_t step = 0x400000;
    uint32_t pos = 0;
    uint32_t pos_max = 0x10000 * SINE_WAVE_TABLE_LEN;
    uint vol = 128;

    // Configure audio playback
    gb_serial_init();

    // Start streaming
    gb_serial_start();

    while (true) {
        int expected;
        while (expected = gb_serial_needs_samples()) {
            int16_t *samples = malloc(sizeof(int16_t) * expected);
            for (uint i = 0; i < expected; i++) {
                samples[i] = (vol * sine_wave_table[pos >> 16u]) >> 8u;
                pos += step;
                if (pos >= pos_max) pos -= pos_max;
            }
            step = (step + 0x1000) % 0x1000000;
            gb_serial_fill_buffer(samples);
            free(samples);
        }
        sleep_ms(POLL_RATE_MS);
    }
}