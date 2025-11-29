#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "bsp/board_api.h"

#include "gb_serial.h"
#include "gb_audio.h"
#include "usb_audio.h"
#include "cover.h"

// Declare pins in binary info
bi_decl(bi_3pins_with_names(
    PIN_SERIAL_CLOCK, "GB Serial Clock",
    PIN_SERIAL_MISO, "GB Serial IN",
    PIN_SERIAL_MOSI, "GB Serial OUT"
));


#define POLL_RATE_MS 1

char artist[] = "USB UAC2";
char title[] = "Audio interface";

int main(void) {
    stdio_init_all();
    
    board_init();

    sleep_ms(1000);

    printf("USB Player NEW\n");

#if ENABLE_DOUBLE_SPEED == 1
    printf("Playback rate: 16384Hz\n");
#else
    printf("Playback rate: 8192Hz\n");
#endif

    // Init USB audio interface
    usb_audio_init();

    if (board_init_after_tusb) {
        board_init_after_tusb();
    }

    // Configure audio playback
    gb_serial_init();

    gb_audio_new_track_blocking(cover_tiles_usb, artist, title);

    // Start streaming
    printf("Start streaming audio samples\n");
    gb_audio_streaming_start(32);

    while (true) {
        usb_audio_tasks();
        int expected;
        while (expected = gb_audio_streaming_needs_samples()) {
            // Expect stereo samples from source
            int expected_bytes = sizeof(int16_t) * 2 * expected;
            int16_t *samples = malloc(expected_bytes);
            usb_audio_read_samples(samples, expected_bytes);

            // Mix samples to mono
            int16_t i;
            for (i=0; i<expected; i++) {
                samples[i] = (samples[2*i] / 2.0f) + (samples[2*i+1] / 2.0f);
            }
            
            gb_audio_streaming_fill_buffer(samples);
            free(samples);
        }
        sleep_ms(POLL_RATE_MS);
    }
}

