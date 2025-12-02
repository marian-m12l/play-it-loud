#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"

#include "gb_serial.h"
#include "gb_audio.h"
#include "cover.h"
#include "samples.h"

// Declare pins in binary info
bi_decl(bi_3pins_with_names(
    PIN_SERIAL_CLOCK, "GB Serial Clock",
    PIN_SERIAL_MISO, "GB Serial IN",
    PIN_SERIAL_MOSI, "GB Serial OUT"
));


#define POLL_RATE_MS 1

char artist[] = "Static audio";
char title[] = "Pre-recorded";

int main() {
    stdio_init_all();

    sleep_ms(1000);

    printf("Static Samples Player\n");

    // Configure audio playback
    gb_audio_init();

    gb_audio_new_track_blocking(false, cover_tiles_sine, artist, title);

    // Start streaming
    int buffers_size = (int) (2 * gb_audio_playback_rate() / 1000.0f);  // Each output buffer holds 2 milliseconds worth of samples
    printf("Start streaming audio samples\n");
    gb_audio_streaming_start(44100, buffers_size);

    uint32_t pos = 0;

    while (true) {
        int expected;
        while (expected = gb_audio_streaming_needs_samples()) {
            int16_t *samples = malloc(sizeof(int16_t) * expected);
            for (uint i = 0; i < expected; i++) {
                samples[i] = audio_samples[pos];
                pos++;
                if (pos >= audio_samples_len) pos -= audio_samples_len;
            }
            gb_audio_streaming_fill_buffer(samples);
            free(samples);
        }
        sleep_ms(POLL_RATE_MS);
    }
}