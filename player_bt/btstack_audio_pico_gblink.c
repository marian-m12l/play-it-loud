#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "pico/stdlib.h"

#define BTSTACK_FILE__ "btstack_audio_pico.c"
#include "btstack_config.h"
#include "btstack_debug.h"
#include "btstack_audio.h"
#include "btstack_run_loop.h"

#include "gb_serial.h"
#include "avrcp.h"


#define DRIVER_POLL_INTERVAL_MS   5

// client
static void (*playback_callback)(int16_t * buffer, uint16_t num_samples);

// timer to fill output ring buffer
static btstack_timer_source_t  driver_timer_sink;

static bool btstack_audio_pico_sink_active;
static uint8_t btstack_audio_pico_channel_count;

//bool dma_started = false;


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
    while (expected = gb_serial_streaming_needs_samples()) {
        int16_t *samples = malloc(sizeof(int16_t) * btstack_audio_pico_channel_count * expected);
        (*playback_callback)(samples, expected);  // TODO expected == SAMPLES_PER_BUFFER / 512 ?

        // Mix samples to mono
        if (btstack_audio_pico_channel_count == 2) {
            int16_t i;
            for (i=0; i<expected; i++) {
                samples[i] = (samples[2*i] / 2.0f) + (samples[2*i+1] / 2.0f);
            }
        }

        gb_serial_streaming_fill_buffer(samples);
        free(samples);
    }
}

static void driver_timer_handler_sink(btstack_timer_source_t * ts) {
    // TODO Pause when changing track
    /*if (avrcp_is_playing() && avrcp_track_changed()) {
        printf("\n\nTRACK CHANGED: PAUSING TO UPLOAD COVER AND METADATA\n\n");
        avrcp_pause();
    }*/

    // refill
    btstack_audio_pico_sink_fill_buffers();

    // re-set timer
    btstack_run_loop_set_timer(ts, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(ts);
}

static void btstack_audio_pico_sink_set_volume(uint8_t volume) {
    //printf("btstack_audio_pico_sink_set_volume: %d\n", volume);
}


bool cover_started_tx = false;
bool metadata_started_tx = false;
//int cover_index;
char metadata[36];

// TODO Simpler version of cover timer handler
// FIXME Different interval?
static void timer_handler_cover(btstack_timer_source_t * ts) {
    if (!avrcp_cover_loaded()) {
        // Cover is NOT loaded: try again later
    } else {
        // Cover is loaded
        if (!cover_started_tx) {
            // Transfer the whole cover in one go
            printf("Cover loaded: start transfer of 2704 bytes\n");
            gb_serial_immediate_transfer(avrcp_cover_tiles(), 2704);
            cover_started_tx = true;
        } else {
            if (!metadata_started_tx) {
                if (!gb_serial_transfer_done()) {
                    // Cover transfer ongoing
                } else {
                    // Cover transferred, send track and artist
                    printf("Cover transferred: start transfer of 36 bytes of metadata (artist: %s)(track: %s)\n", avrcp_get_artist(), avrcp_get_track());
                        
                    // FIXME Shouldn't be needed, DMA should self-deinit when transfer is done (wit hirq handler ?)
                    //gb_serial_stop(false);
                    
                    // Characters must be uppercased and shifted -0x20 because char tiles are available as 0x00-0x3f (mapped to 0x20-0x5f ascii)
                    memset(metadata, ' ', 36);
                    int len = strlen(avrcp_get_artist());
                    len = len > 18 ? 18 : len;
                    memcpy(metadata, avrcp_get_artist(), len);
                    len = strlen(avrcp_get_track());
                    len = len > 18 ? 18 : len;
                    memcpy(&metadata[18], avrcp_get_track(), len);
                    printf("metadata: %s\n", metadata);
                    for (int i=0; i<36; i++) {
                        metadata[i] = toupper(metadata[i]) - 0x20;
                    }
                    gb_serial_immediate_transfer(metadata, 36);
                    metadata_started_tx = true;
                }
            } else {
                if (!gb_serial_transfer_done()) {
                    // Metadata transfer ongoing
                } else {
                    // Done. Now fill audio buffers and start audio samples DMA
                    printf("Metadata transferred: start audio samples DMA\n");
                        
                    // FIXME Shouldn't be needed, DMA should self-deinit when transfer is done (wit hirq handler ?)
                    //gb_serial_stop(false);

                    // pre-fill HAL buffers
                    btstack_audio_pico_sink_fill_buffers(); // FIXME Is source is paused this can be blocking ?

                    // start timer
                    btstack_run_loop_set_timer_handler(&driver_timer_sink, &driver_timer_handler_sink);
                    btstack_run_loop_set_timer(&driver_timer_sink, DRIVER_POLL_INTERVAL_MS);
                    btstack_run_loop_add_timer(&driver_timer_sink);

                    // state
                    btstack_audio_pico_sink_active = true;

                    gb_serial_streaming_start();

                    // Do NOT reset this timer handler. Now we rely on driver_timer_handler_sink to fill audio buffers
                    return;
                }
            }
        }
    }

    btstack_run_loop_set_timer(ts, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(ts);
}

static void btstack_audio_pico_sink_start_stream(void) {
    printf("Start streaming over GB Link\n");

    // start timer
    btstack_run_loop_set_timer_handler(&driver_timer_sink, &timer_handler_cover);
    btstack_run_loop_set_timer(&driver_timer_sink, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(&driver_timer_sink);

}

static void btstack_audio_pico_sink_stop_stream(void) {
    printf("Stop streaming over GB Link\n");    // Actually streaming silence --> stop when enough zeroes sent to fill gb buffers ?

    // stop timer
    btstack_run_loop_remove_timer(&driver_timer_sink);
    // state
    btstack_audio_pico_sink_active = false;

    // FIXME Send 0xf1 to reset game, then wait for new track/cover ?

    gb_serial_streaming_stop();
    //dma_started = false;


    // TODO GAME RESET SHOULD NOT BE TRIGGERRED BY STREAM STOP (WHICH IS JUST A PAUSE)

    printf("Tranferring reset trigger: start transfer of 1 byte\n");
    uint8_t trigger_reset = 0xf1;
    gb_serial_immediate_transfer(&trigger_reset, 1);
    // FIXME Should not be blocking ?? but one byte is sent quickly...
    while (!gb_serial_transfer_done()) {
        tight_loop_contents();
    }
    printf("Sent reset trigger 0xf1\n");
    // FIXME Read ack from the GB (which should set some ack value in rSB in GameReset and wait long enough before reading cover tiles)

    // FIXME Shouldn't be needed, DMA should self-deinit when transfer is done (wit hirq handler ?)
    //gb_serial_stop(false);

    // TODO reset cover and dma tx ??
    cover_started_tx = false;
    metadata_started_tx = false;

    // FIXME Same when track changes ??

    // Clear buffers
    gb_serial_streaming_clear_buffers();
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
