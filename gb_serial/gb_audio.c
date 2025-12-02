#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "pico/stdlib.h"

#include "gb_serial.h"
#include "downsampler.h"
#include "encoder.h"

#include "gb_audio.h"

#define OUTPUT_SAMPLE_RATE_BASE 11025
#define OUTPUT_SAMPLE_RATE_BASE_HQ 16000

#define COVER_TILES_BYTES (14*13*16)
#define RESET_TRIGGER_DELAY_MS 100
#define PRE_STREAM_DELAY_MS 1000
#define METADATA_LENGTH 36

#define BUFFER_COUNT 3
unsigned char* samples_buffers[BUFFER_COUNT];

int filling_buffer = 0;
int playback_buffer = BUFFER_COUNT-1;

downsampler_t downsampler_instance;
encoder_t encoder_instance;

uint8_t reset_trigger = 0xf1;
char metadata[METADATA_LENGTH];
uint64_t timestamp;

uint16_t playback_rate;
bool playback_hq;


void swap() {
    playback_buffer = (playback_buffer + 1) % BUFFER_COUNT;
}

uint8_t* get_playback_buffer() {
    return samples_buffers[playback_buffer];
}

gb_serial_source_t source = {
    .buffer_size = 0,   // Set when starting to stream
    .swap = &swap,
    .playback_buffer = &get_playback_buffer
};

void gb_audio_init() {
    // Initialize serial and probe console/rom to determine its capabilities
    gb_serial_init(2048);
    gb_audio_new_track_step1();
    while (!gb_audio_new_track_step1_done()) {
        tight_loop_contents();
    }
    uint8_t capabilities = gb_serial_received();
    // HQ encoding
    playback_hq = (capabilities & 0x02);
    printf("Audio playback HQ: %d\n", playback_hq);
    if (playback_hq) {
        reset_trigger = 0xff;
    }
    playback_rate = playback_hq ? OUTPUT_SAMPLE_RATE_BASE_HQ : OUTPUT_SAMPLE_RATE_BASE;
    // Double speed
    if (capabilities & 0x01) {
        playback_rate *= 2;
    }
    printf("Audio playback rate: %d Hz\n", playback_rate);
    gb_serial_set_transfer_rate(playback_rate);
}

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
    return gb_serial_transfer_done() && (time_us_64() - timestamp) > PRE_STREAM_DELAY_MS*1000;
}

void gb_audio_new_track_blocking(bool reset, const uint8_t* cover_tiles, const char* artist, const char* title) {
    if (reset) {
        gb_audio_new_track_step1();
        while (!gb_audio_new_track_step1_done()) {
            tight_loop_contents();
        }
    }
    gb_audio_new_track_step2(cover_tiles);
    while (!gb_audio_new_track_step2_done()) {
        tight_loop_contents();
    }
    gb_audio_new_track_step3(artist, title);
    while (!gb_audio_new_track_step3_done()) {
        tight_loop_contents();
    }
}

// FIXME buffer size in ms ??
void gb_audio_streaming_start(uint16_t input_sample_rate, int output_buffers_size) {
    // FIXME Assert input_sample_rate >= playback_rate
    downsample_init(&downsampler_instance, input_sample_rate, playback_rate);
    encode_init(&encoder_instance);
    for (int i=0; i<BUFFER_COUNT; i++) {
        samples_buffers[i] = malloc(output_buffers_size);
    }
    source.buffer_size = output_buffers_size;
    gb_audio_streaming_clear_buffers();
    gb_serial_streaming_start(&source);
}

void gb_audio_streaming_stop() {
    gb_serial_streaming_stop();
    for (int i=0; i<BUFFER_COUNT; i++) {
        free(samples_buffers[i]);
    }
}

bool gb_audio_streaming_is_busy() {
    return gb_serial_streaming_is_busy();
}

int gb_audio_streaming_needs_samples() {
    return (filling_buffer != playback_buffer) ? downsample_expected_samples(&downsampler_instance, source.buffer_size) : 0;
}

void gb_audio_streaming_fill_buffer(int16_t* samples) {
    // Downsample and encode the next samples
    int16_t* downsampledAudioData = malloc(source.buffer_size * sizeof(int16_t));
    unsigned char* convertedAudioData = malloc(source.buffer_size);
    uint32_t samples_before = downsampler_instance.samples;
    
    // Downsample
    // FIXME Skip downsampling if input_sample_rate == playback_rate
    downsample(&downsampler_instance, samples, downsampledAudioData, downsample_expected_samples(&downsampler_instance, source.buffer_size));
    // Convert int16_t to unsigned char
    for(int i = 0; i < downsampler_instance.samples-samples_before; i++) {
        convertedAudioData[i] = (downsampledAudioData[i] + 32768) >> 8;
    }
    // Encode
    if (playback_hq) {
        encode_hq(&encoder_instance, convertedAudioData, samples_buffers[filling_buffer], downsampler_instance.samples-samples_before);
    } else {
        encode(&encoder_instance, convertedAudioData, samples_buffers[filling_buffer], downsampler_instance.samples-samples_before);
    }

    free(downsampledAudioData);
    free(convertedAudioData);

    // Swap buffers
    filling_buffer = (filling_buffer + 1) % BUFFER_COUNT;
}

void gb_audio_streaming_fill_buffer_encoded(uint8_t* raw, int len) {
    memcpy(samples_buffers[filling_buffer], raw, len);
    filling_buffer = (filling_buffer + 1) % BUFFER_COUNT;
}

void gb_audio_streaming_clear_buffers() {
    for (int i=0; i<BUFFER_COUNT; i++) {
        memset(samples_buffers[i], 0x80, source.buffer_size);
    }
}
