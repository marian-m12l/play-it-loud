#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "bsp/board_api.h"

#include "gb_serial.h"
#include "usb_audio.h"

// Declare pins in binary info
bi_decl(bi_3pins_with_names(
    PIN_SERIAL_CLOCK, "GB Serial Clock",
    PIN_SERIAL_MISO, "GB Serial IN",
    PIN_SERIAL_MOSI, "GB Serial OUT"
));


#define POLL_RATE_MS 5

// TODO USB Audio Interface ?
// TODO Stream audio

int main() {
    stdio_init_all();
    
    // TODO Needed???
    board_init();

    printf("USB Player\n");

    // Init USB audio interface
    usb_audio_init();

    // TODO Needed???
    if (board_init_after_tusb) {
        board_init_after_tusb();
    }

    // Configure audio playback
    gb_serial_init();

    // Start streaming
    gb_serial_streaming_start();

    while (true) {
        usb_audio_tasks();
        int expected;
        while (expected = gb_serial_streaming_needs_samples()) {
            int16_t *samples = malloc(sizeof(int16_t) * expected);
            // TODO Get samples from usb audio interface incoming samples
            usb_audio_read_samples(samples, expected);
            gb_serial_streaming_fill_buffer(samples);
            free(samples);
        }
    }
}