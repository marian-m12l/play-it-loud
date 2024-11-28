#pragma once

#include <stdint.h>


void gb_audio_new_track_step1();
bool gb_audio_new_track_step1_done();
void gb_audio_new_track_step2(uint8_t* cover_tiles);
bool gb_audio_new_track_step2_done();
void gb_audio_new_track_step3(char* artist, char* title);
bool gb_audio_new_track_step3_done();

void gb_audio_streaming_start();
void gb_audio_streaming_stop();
int gb_audio_streaming_needs_samples();
void gb_audio_streaming_fill_buffer(int16_t* samples);
void gb_audio_streaming_clear_buffers();
