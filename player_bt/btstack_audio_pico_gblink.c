#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BTSTACK_FILE__ "btstack_audio_pico.c"
#include "btstack_config.h"
#include "btstack_debug.h"
#include "btstack_audio.h"
#include "btstack_run_loop.h"

#include "gb_serial.h"


#define DRIVER_POLL_INTERVAL_MS   5

// client
static void (*playback_callback)(int16_t * buffer, uint16_t num_samples);

// timer to fill output ring buffer
static btstack_timer_source_t  driver_timer_sink;

static bool btstack_audio_pico_sink_active;
static uint8_t btstack_audio_pico_channel_count;

bool dma_started = false;


static int btstack_audio_pico_sink_init(uint8_t channels, uint32_t samplerate, void (*playback)(int16_t * buffer, uint16_t num_samples)) {
    btstack_assert(playback != NULL);
    btstack_assert(channels != 0);

    playback_callback = playback;

    // num channels requested by application
    btstack_audio_pico_channel_count = channels;

    // Configure audio playback
    gb_serial_init();

    return 0;
}

static void btstack_audio_pico_sink_fill_buffers(void) {
    int expected;
    while (expected = gb_serial_needs_samples()) {
        int16_t *samples = malloc(sizeof(int16_t) * btstack_audio_pico_channel_count * expected);
        (*playback_callback)(samples, expected);  // TODO expected == SAMPLES_PER_BUFFER / 512 ?

        // Mix samples to mono
        if (btstack_audio_pico_channel_count == 2) {
            int16_t i;
            for (i=0; i<expected; i++) {
                samples[i] = (samples[2*i] / 2.0f) + (samples[2*i+1] / 2.0f);
            }
        }

        gb_serial_fill_buffer(samples);
        free(samples);
    }
}

static void driver_timer_handler_sink(btstack_timer_source_t * ts) {
    // refill
    btstack_audio_pico_sink_fill_buffers();

    // re-set timer
    btstack_run_loop_set_timer(ts, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(ts);
}

static void btstack_audio_pico_sink_set_volume(uint8_t volume) {
    //printf("btstack_audio_pico_sink_set_volume: %d\n", volume);
}

static void btstack_audio_pico_sink_start_stream(void) {
    printf("Start streaming over GB Link\n");

    // pre-fill HAL buffers
    btstack_audio_pico_sink_fill_buffers();

    // start timer
    btstack_run_loop_set_timer_handler(&driver_timer_sink, &driver_timer_handler_sink);
    btstack_run_loop_set_timer(&driver_timer_sink, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(&driver_timer_sink);

    // state
    btstack_audio_pico_sink_active = true;

    // TODO Should be handled by the gb_serial lib?
    if (dma_started) {
        printf("DMA is already running");
        // TODO Reset buffers and restart transfers?
    } else {
        printf("Starting DMA\n");

        // Start streaming
        gb_serial_start();

        dma_started = true;
    }
}

static void btstack_audio_pico_sink_stop_stream(void) {
    printf("Stop streaming over GB Link\n");

    // Clear buffers
    gb_serial_clear_buffers();

    // stop timer
    btstack_run_loop_remove_timer(&driver_timer_sink);
    // state
    btstack_audio_pico_sink_active = false;
}

static void btstack_audio_pico_sink_close(void) {
    // stop stream if needed
    if (btstack_audio_pico_sink_active){
        btstack_audio_pico_sink_stop_stream();
    }
}

static const btstack_audio_sink_t btstack_audio_pico_sink = {
    .init = &btstack_audio_pico_sink_init,
    .set_volume = &btstack_audio_pico_sink_set_volume,
    .start_stream = &btstack_audio_pico_sink_start_stream,
    .stop_stream = &btstack_audio_pico_sink_stop_stream,
    .close = &btstack_audio_pico_sink_close,
};

const btstack_audio_sink_t * btstack_audio_pico_sink_get_instance(void) {
    return &btstack_audio_pico_sink;
}
