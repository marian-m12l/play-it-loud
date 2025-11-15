#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"

#include "gb_serial.h"
#include "gb_audio.h"
#include "cover.h"
#include "samples.h"
#include "encoded.h"

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

    printf("Static audio Player\n");

    // Configure audio playback
    gb_serial_init();

    gb_audio_new_track_blocking(cover_tiles_sine, artist, title);

    // Start streaming
    printf("Start streaming audio samples\n");
    gb_audio_streaming_start();

    uint32_t pos = 0;

    // TODO Just stream already encoded data ???

    while (true) {
        int expected;
        while (expected = gb_audio_streaming_needs_samples()) {
            // FIXME Input samples must be 44100 hz !!!

            // TODO Read samples and convert/encode or read encoded data ??
            // FIXME Log expected samples and timings

            // 5.2 ms to resample and encode 441 samples (10ms worth of audio) !!!
            
            //uint64_t before = time_us_64();
            int16_t *samples = malloc(sizeof(int16_t) * expected);
            for (uint i = 0; i < expected; i++) {
                samples[i] = audio_samples[pos];
                pos++;
                if (pos >= audio_samples_len) pos -= audio_samples_len;
            }
            //uint64_t mid = time_us_64();
            // TODO How fast/slow is resampling and encoding ??
            gb_audio_streaming_fill_buffer(samples, expected);
            free(samples);
            //uint64_t after = time_us_64();
            //printf("%llu expected=%d pos=%d time=%llu\n", after, expected, pos, after-before);
            

            // TODO Pre-encoded sounds good, no clicks
            // TODO Test with encoding from 44100hz samples ??
            // TODO Test resampler + encoder --> pregenerate and plyback encoded bytes ?

            /*uint8_t *samples = malloc(sizeof(uint8_t) * 164);
            for (uint i = 0; i < 164; i++) {
                samples[i] = encoded_bytes[pos];
                pos++;
                if (pos >= encoded_len) pos -= encoded_len;
            }
            // TODO How fast/slow is resampling and encoding ??
            gb_audio_streaming_fill_buffer_encoded(samples, 164);
            free(samples);*/
        }
        sleep_ms(POLL_RATE_MS);
    }
}