#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "downsampler.h"

void downsample_init(downsampler_t* instance, uint16_t input_sample_rate, uint16_t output_sample_rate) {
    instance->samples = 0;
    instance->prev_output = 0;
    instance->prev_sample = 0;
    instance->curr_sample = 0;
    instance->decimate_factor = 1.0 * input_sample_rate / output_sample_rate;
    instance->phase = 0.0;
    instance->rate = 1.0 / instance->decimate_factor;
    // fc = half of target sample rate
    // dt = inverse of input sample rate
    // RC = 1/(2*PI*fc)
    // ALPHA = dt / ( dt + RC )
    instance->alpha_factor = (1.0/input_sample_rate) / ( (1.0/input_sample_rate) + (1.0/(M_PI*output_sample_rate)) );
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
        // Decimate / downsample by an decimal factor
        instance->phase += instance->rate;
        if (instance->phase >= 1.0) {
            instance->phase -= 1.0;
            output[j++] = filtered;
        }
        instance->prev_sample = instance->curr_sample;
        instance->prev_output = filtered;
    }
    instance->samples += j;
}

// Calculates how many input samples are needed to produce the required number of output samples
// (depending on the internal state of the downsampler)
int downsample_expected_samples(downsampler_t* instance, int output_samples) {
    return (int) ceil(1.0 * output_samples * instance->decimate_factor - instance->phase);
}
