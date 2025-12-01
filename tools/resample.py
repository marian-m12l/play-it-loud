#!/usr/bin/env python3

import wave
import array
import itertools
import math
import scipy.signal as sps

#target_sample_rate = 16000
target_sample_rate = 12483
#target_sample_rate = 8000
#target_sample_rate = 6241

with wave.open("input.wav", mode="rb") as input_file:
    # Read samples and metadata
    metadata = input_file.getparams()
    frames = input_file.readframes(metadata.nframes)
    pcm_samples = array.array("h", frames)
    # Merge stereo tracks into mono track
    merged = [(l/2 + r/2) for l,r in itertools.batched(pcm_samples, 2)]
    # Low-pass filter and downsample
    # fc = half of target sample rate
    # dt = inverse of input sample rate
    # RC = 1/(2*PI*fc)
    # ALPHA = dt / ( dt + RC )
    input_sample_rate = metadata.framerate
    decimate_factor = (1.0*input_sample_rate)/target_sample_rate
    phase = 0.0
    rate = 1.0 / decimate_factor
    alpha_factor = (1.0/input_sample_rate) / ( (1.0/input_sample_rate) + (1.0/(math.pi*target_sample_rate)) )
    downsampled = []
    first = True
    prev_sample = 0
    for curr_sample in merged:
        # Apply low-pass filter
        if first:
            prev_output = int(alpha_factor * curr_sample)
            first = False
            downsampled.append(prev_output);
            # FIXME Should also set prev_sample to merged[i] ??
        else:
            #filtered = int((1-alpha_factor)*prev_output + alpha_factor*(curr_sample+prev_sample)/2)
            filtered = int((1-alpha_factor)*prev_output + alpha_factor*curr_sample)
            # Decimate / downsample by a decimal factor
            phase += rate;
            if phase >= 1.0:
                phase -= 1.0;
                downsampled.append(filtered)
            prev_sample = curr_sample
            prev_output = filtered
    # Convert samples to 8-bit int
    encoded = [int(s+32768) >> 8 for s in downsampled]
    # Output wave file
    with wave.open("output.wav", mode="wb") as output_file:
        output_file.setnchannels(1)
        output_file.setsampwidth(1)
        output_file.setframerate(target_sample_rate)
        output_file.writeframes(bytes(encoded))

    # FIXME SciPy --> barely better quality at 16000Hz
    # Resample data
    number_of_samples = round(len(merged) * float(target_sample_rate) / input_sample_rate)
    downsampled = sps.resample(merged, number_of_samples)
    # Convert samples to 8-bit int
    encoded = [int(s+32768) >> 8 for s in downsampled]
    # Output wave file
    with wave.open("output_scipy.wav", mode="wb") as output_file:
        output_file.setnchannels(1)
        output_file.setsampwidth(1)
        output_file.setframerate(target_sample_rate)
        output_file.writeframes(bytes(encoded))
