#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "downsampler.h"

void downsample_init(downsampler_t* instance, uint16_t input_sample_rate, uint8_t decimate_factor) {
    instance->samples = 0;
    instance->prev_output = 0;
    instance->prev_sample = 0;
    instance->curr_sample = 0;
    instance->decimate_factor = decimate_factor;
    // fc = half of target sample rate
    // dt = inverse of input sample rate
    // RC = 1/(2*PI*fc)
    // ALPHA = dt / ( dt + RC )
    double target_sample_rate = input_sample_rate / decimate_factor;
    instance->alpha_factor = (1.0/input_sample_rate) / ( (1.0/input_sample_rate) + (1.0/(M_PI*target_sample_rate)) );
}

void downsample(downsampler_t* instance, const int16_t input[], int16_t * output, int length) {
    int i=0, j=0;
    
    // First sample
    if (instance->samples == 0) {
        instance->prev_output = instance->alpha_factor * input[i++];
        output[j++] = instance->prev_output;
    }
    for(; i < length; i++) {
        instance->curr_sample = input[i];
        // Apply low-pass filter
        int16_t filtered = (1-instance->alpha_factor)*instance->prev_output + instance->alpha_factor*(instance->curr_sample+instance->prev_sample)/2;
        // Decimate / downsample by an integer factor
        if (i % instance->decimate_factor == 0) {
            output[j++] = filtered;
        }
        instance->prev_sample = instance->curr_sample;
        instance->prev_output = filtered;
    }
    instance->samples += j;
}
