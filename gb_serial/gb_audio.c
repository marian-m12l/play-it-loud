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

// Each buffer holds 2 milliseconds worth of samples
#define BUFFER_SIZE ((int)(1.0*OUTPUT_SAMPLE_RATE/1000*2))
#define BUFFER_COUNT 3
#define EXPECTED_SAMPLES ((int)((1.0*INPUT_SAMPLE_RATE/OUTPUT_SAMPLE_RATE)*BUFFER_SIZE))
unsigned char samples_buffers[BUFFER_COUNT][BUFFER_SIZE];

int filling_buffer = 0;
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
    gb_serial_immediate_transfer(metadata, METADATA_LENGTH);
}
bool gb_audio_new_track_step3_done() {
    return gb_serial_transfer_done();
}

void gb_audio_new_track_blocking(const uint8_t* cover_tiles, const char* artist, const char* title) {
    do {
        gb_audio_new_track_step1();
        while (!gb_audio_new_track_step1_done()) {
            tight_loop_contents();
        }
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
    downsample_init(&downsampler_instance, INPUT_SAMPLE_RATE, OUTPUT_SAMPLE_RATE);
    encode_init(&encoder_instance);
    gb_serial_streaming_start(&source);
}

void gb_audio_streaming_stop() {
    gb_serial_streaming_stop();
}

bool gb_audio_streaming_is_busy() {
    return gb_serial_streaming_is_busy();
}

int gb_audio_streaming_needs_samples() {
    return (filling_buffer != playback_buffer) ? EXPECTED_SAMPLES : 0;
}

void gb_audio_streaming_fill_buffer(int16_t* samples) {
    // Downsample and encode the next samples
    int16_t downsampledAudioData[BUFFER_SIZE];
    unsigned char convertedAudioData[BUFFER_SIZE];
    uint32_t samples_before = downsampler_instance.samples;
    
    // Downsample
    downsample(&downsampler_instance, samples, downsampledAudioData, EXPECTED_SAMPLES);
    // Convert int16_t to unsigned char
    for(int i = 0; i < downsampler_instance.samples-samples_before; i++) {
        convertedAudioData[i] = (downsampledAudioData[i] + 32768) >> 8;
    }
    // Encode
    encode(&encoder_instance, convertedAudioData, samples_buffers[filling_buffer], downsampler_instance.samples-samples_before);

    // Swap buffers
    filling_buffer = (filling_buffer + 1) % BUFFER_COUNT;
}

void gb_audio_streaming_clear_buffers() {
    for (int i=0; i<BUFFER_COUNT; i++) {
        memset(samples_buffers[i], 0, BUFFER_SIZE);
    }
}
