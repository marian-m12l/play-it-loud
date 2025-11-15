#include <string.h>
#include <ctype.h>
#include "pico/stdlib.h"

#include "gb_serial.h"
#include "downsampler.h"
#include "encoder.h"

#include "gb_audio.h"

#define INPUT_SAMPLE_RATE 44100

#if ENABLE_DOUBLE_SPEED == 1
#define OUTPUT_SAMPLE_RATE 16384
#else
#define OUTPUT_SAMPLE_RATE 8192
#endif

#define COVER_TILES_BYTES (14*13*16)
#define RESET_TRIGGER_DELAY_MS 100
#define METADATA_LENGTH 36
#define METADATA_TO_PLAYBACK_DELAY_MS 1000

// Each buffer holds 2 milliseconds worth of samples
#define BUFFER_SIZE 164 // FIXME ((int)(1.0*OUTPUT_SAMPLE_RATE/1000*10))
#define BUFFER_COUNT 3
#define EXPECTED_SAMPLES 441 // FIXME ((int)((1.0*INPUT_SAMPLE_RATE/OUTPUT_SAMPLE_RATE)*BUFFER_SIZE))    // FIXME Always 88 (44100/1000*2) ?? 441 ???
unsigned char samples_buffers[BUFFER_COUNT][BUFFER_SIZE];

int filling_buffer = 0;
int filling_buffer_read_offset = 0;
int filling_buffer_write_offset = 0;
int playback_buffer = BUFFER_COUNT-1;

downsampler_t downsampler_instance;
encoder_t encoder_instance;

uint8_t reset_trigger = 0xf1;
char metadata[METADATA_LENGTH];
uint64_t timestamp;


void swap() {
    playback_buffer = (playback_buffer + 1) % BUFFER_COUNT;
}

uint8_t* get_playback_buffer() {
    return samples_buffers[playback_buffer];
}

gb_serial_source_t source = {
    .buffer_size = BUFFER_SIZE,
    .swap = &swap,
    .playback_buffer = &get_playback_buffer
};

void gb_audio_new_track_step1() {
    timestamp = time_us_64();
    gb_serial_immediate_transfer(&reset_trigger, 1);
}
bool gb_audio_new_track_step1_done() {
    return gb_serial_transfer_done() && (time_us_64() - timestamp) > RESET_TRIGGER_DELAY_MS*1000;
}
void gb_audio_new_track_step2(const uint8_t* cover_tiles) {
    gb_serial_immediate_transfer(cover_tiles, COVER_TILES_BYTES);
}
bool gb_audio_new_track_step2_done() {
    return gb_serial_transfer_done();
}
void gb_audio_new_track_step3(const char* artist, const char* title) {
    // Characters must be uppercased and shifted -0x20 because char tiles are available as 0x00-0x3f (mapped to 0x20-0x5f ascii)
    memset(metadata, ' ', METADATA_LENGTH);
    int len = strlen(artist);
    len = len > METADATA_LENGTH/2 ? METADATA_LENGTH/2 : len;
    memcpy(metadata, artist, len);
    len = strlen(title);
    len = len > METADATA_LENGTH/2 ? METADATA_LENGTH/2 : len;
    memcpy(&metadata[METADATA_LENGTH/2], title, len);
    for (int i=0; i<METADATA_LENGTH; i++) {
        metadata[i] = toupper(metadata[i]) - 0x20;
    }
    timestamp = time_us_64();
    gb_serial_immediate_transfer(metadata, METADATA_LENGTH);
}
bool gb_audio_new_track_step3_done() {
    // FIXME cover transfer takes about 180 ms IN STEP 2 !!
    // FIXME metadata transfer should be way faster ?!
    // FIXME How many cycles between metadata and console ready for playback ?

    // FIXME Wait for n milliseconds AFTER TRANSFER IS DONE for playback to be ready on console (--> count cycles !!)
    return gb_serial_transfer_done() && (time_us_64() - timestamp) > METADATA_TO_PLAYBACK_DELAY_MS*1000;
}

void gb_audio_new_track_blocking(const uint8_t* cover_tiles, const char* artist, const char* title) {
    do {
        gb_audio_new_track_step1();
        while (!gb_audio_new_track_step1_done()) {
            tight_loop_contents();
        }
        // FIXME remove debug logs
        printf("Serial response to reset trigger: 0x%02x\n", gb_serial_received());
    } while (gb_serial_received() != 0xf2);
    gb_audio_new_track_step2(cover_tiles);
    while (!gb_audio_new_track_step2_done()) {
        tight_loop_contents();
    }
    gb_audio_new_track_step3(artist, title);
    while (!gb_audio_new_track_step3_done()) {
        tight_loop_contents();
    }
}

void gb_audio_streaming_start() {
    // TODO Make input/output samples rates configurable ? malloc appropriate buffers
    downsample_init(&downsampler_instance, INPUT_SAMPLE_RATE, OUTPUT_SAMPLE_RATE);
    encode_init(&encoder_instance);
    gb_audio_streaming_clear_buffers();
    gb_serial_streaming_start(&source);
}

void gb_audio_streaming_stop() {
    gb_serial_streaming_stop();
}

bool gb_audio_streaming_is_busy() {
    return gb_serial_streaming_is_busy();
}

int gb_audio_streaming_needs_samples() {
    return (filling_buffer != playback_buffer) ? EXPECTED_SAMPLES - filling_buffer_read_offset : 0;
}

void gb_audio_streaming_fill_buffer(int16_t* samples, int len) {
    //printf("gb_audio_streaming_fill_buffer: len=%d filling_buffer=%d filling_buffer_write_offset=%d write_to=%p filling_buffer_read_offset=%d playback_buffer=%d\n", len, filling_buffer, filling_buffer_write_offset, samples_buffers[filling_buffer]+filling_buffer_write_offset, filling_buffer_read_offset, playback_buffer);

    // Downsample and encode the next samples
    int16_t downsampledAudioData[BUFFER_SIZE];
    unsigned char convertedAudioData[BUFFER_SIZE];
    uint32_t samples_before = downsampler_instance.samples;
    
    // Downsample
    downsample(&downsampler_instance, samples, downsampledAudioData, len);
    // Convert int16_t to unsigned char
    for(int i = 0; i < downsampler_instance.samples-samples_before; i++) {
        convertedAudioData[i] = (downsampledAudioData[i] + 32768) >> 8;
    }
    // Encode
    encode(&encoder_instance, convertedAudioData, samples_buffers[filling_buffer]+filling_buffer_write_offset, downsampler_instance.samples-samples_before);

    filling_buffer_read_offset += len;
    filling_buffer_write_offset += (downsampler_instance.samples-samples_before);
    //printf("filling_buffer_write_offset=%d filling_buffer_read_offset=%d\n", filling_buffer_write_offset, filling_buffer_read_offset);
    if (filling_buffer_read_offset >= EXPECTED_SAMPLES) {
        // Swap buffers
        filling_buffer = (filling_buffer + 1) % BUFFER_COUNT;
        filling_buffer_read_offset = 0;
        filling_buffer_write_offset = 0;
        //printf("swapped filling_buffer to %d (reset offets)\n", filling_buffer);
    }
}

void gb_audio_streaming_fill_buffer_encoded(uint8_t* raw, int len) {
    memcpy(samples_buffers[filling_buffer], raw, len);
    filling_buffer = (filling_buffer + 1) % BUFFER_COUNT;
}

void gb_audio_streaming_clear_buffers() {
    for (int i=0; i<BUFFER_COUNT; i++) {
        memset(samples_buffers[i], 0x80, BUFFER_SIZE);
    }
}
