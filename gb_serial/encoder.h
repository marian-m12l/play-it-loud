#ifndef _ENCODER_H_
#define _ENCODER_H_

#include <stdint.h>

typedef struct {
    int32_t samples;
    unsigned char curr_config;
    unsigned char prev_config;
    unsigned char curr_sample;
} encoder_t;

void encode_init(encoder_t* instance);
void encode(encoder_t* instance, const unsigned char * input, unsigned char * output, int length);

#endif
