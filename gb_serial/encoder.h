#ifndef _ENCODER_H_
#define _ENCODER_H_

#include <stdint.h>

typedef struct {
    uint8_t pulse_lut[256];
    uint8_t mv_lut[256];
    int amp_lut[256];
} hq_lut_t;

typedef struct {
    int32_t samples;
    unsigned char curr_config;
    unsigned char prev_config;
    unsigned char curr_sample;
    hq_lut_t hq_lut;
} encoder_t;

void encode_init(encoder_t* instance);
void encode(encoder_t* instance, const unsigned char input[], unsigned char * output, int length);
void encode_hq(encoder_t* instance, const unsigned char input[], unsigned char * output, int length);

#endif
