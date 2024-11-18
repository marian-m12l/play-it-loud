#ifndef _DOWNSAMPLER_H_
#define _DOWNSAMPLER_H_

#include <stdint.h>

typedef struct {
    int32_t samples;
    int16_t prev_output;
    int16_t prev_sample;
    int16_t curr_sample;
    uint8_t decimate_factor;
    double alpha_factor;
} downsampler_t;

void downsample_init(downsampler_t* instance, uint16_t input_sample_rate, uint8_t decimate_factor);
void downsample(downsampler_t* instance, const int16_t input[], int16_t * output, int length);

#endif
