
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "../gb_serial/downsampler.h"
#include "../gb_serial/encoder.h"

int main() {

    // Reads data from wav file
    //// WAVE Header Data
    printf("Reading wave header\n");
    char buffer[10];

    fread(buffer, 1, 4, stdin);
    if (memcmp("RIFF", buffer, 4L) != 0) {
        return 2;
    }
    printf("RIFF\n");

    uint32_t chunk_size;
    fread(&chunk_size, 4, 1, stdin);

    printf("chunk_size=%d\n", chunk_size);
    fread(buffer, 1, 4, stdin);
    if (memcmp("WAVE", buffer, 4L) != 0) {
        return 2;
    }
    printf("WAVE\n");

    fread(buffer, 1, 4, stdin);
    if (memcmp("fmt ", buffer, 4L) != 0) {
        return 2;
    }
    printf("fmt \n");

    uint32_t subchunk1_size;
    fread(&subchunk1_size, 4, 1, stdin);
    printf("subchunk1_size=%d\n", subchunk1_size);
    uint16_t fmt_type;
    fread(&fmt_type, 2, 1, stdin);
    printf("fmt_type=%d\n", fmt_type);
    uint16_t chan_num;
    fread(&chan_num, 2, 1, stdin);
    printf("chan_num=%d\n", chan_num);
    uint32_t rate;
    fread(&rate, 4, 1, stdin);
    printf("rate=%d\n", rate);
    uint32_t byte_rate;
    fread(&byte_rate, 4, 1, stdin);
    printf("byte_rate=%d\n", byte_rate);
    uint16_t block_align;
    fread(&block_align, 2, 1, stdin);
    printf("block_align=%d\n", block_align);
    uint16_t bits;
    fread(&bits, 2, 1, stdin);
    printf("bits=%d\n", bits);

    printf("seek=%d\n", 20 + subchunk1_size);
    fseek(stdin, 20 + subchunk1_size, SEEK_SET);
    fread(buffer, 1, 4, stdin);
    if (memcmp("data", buffer, 4L) != 0) {
        return 2;
    }
    printf("data\n");
    uint32_t length;
    fread(&length, 4, 1, stdin);
    printf("length=%d\n", length);

    // Writes data to bin file
    FILE *fp = fopen("encoded_hq.bin", "w");
    if (fp == NULL) {
        printf("Output file couldn't be opened!\n");
        return 4;
    }

    // FIXME From wave file / param
    #define INPUT_SAMPLE_RATE 44100
    #define OUTPUT_SAMPLE_RATE 16384

    // FIXME dynamically !!
    #define BUFFER_SIZE 164
    //#define EXPECTED_SAMPLES 441

    unsigned char samples_buffer[BUFFER_SIZE];

    downsampler_t downsampler_instance;
    encoder_t encoder_instance;

    downsample_init(&downsampler_instance, INPUT_SAMPLE_RATE, OUTPUT_SAMPLE_RATE);
    encode_init(&encoder_instance);

    // Loop, read 10 ms of samples
    for (int pos=0; pos<length;) {
        int to_read = downsample_expected_samples(&downsampler_instance, BUFFER_SIZE)*chan_num*sizeof(int16_t);
        if (length - pos < to_read) {
            to_read = length - pos;
        }
        int16_t *samples = malloc(to_read);
        fread(samples, to_read, 1, stdin);

        // Mix stereo down to mono
        if (chan_num == 2) {
            int16_t i;
            for (i=0; i<(to_read/chan_num)/sizeof(int16_t); i++) {
                samples[i] = (samples[2*i] / 2.0f) + (samples[2*i+1] / 2.0f);
            }
        }

        // Downsample and encode the next samples
        int16_t downsampledAudioData[BUFFER_SIZE];
        unsigned char convertedAudioData[BUFFER_SIZE];
        uint32_t samples_before = downsampler_instance.samples;
        
        // Downsample
        downsample(&downsampler_instance, samples, downsampledAudioData, downsample_expected_samples(&downsampler_instance, BUFFER_SIZE));
        // Convert int16_t to unsigned char
        for(int i = 0; i < downsampler_instance.samples-samples_before; i++) {
            convertedAudioData[i] = (downsampledAudioData[i] + 32768) >> 8;
        }
        // Encode
        encode_hq(&encoder_instance, convertedAudioData, samples_buffer, downsampler_instance.samples-samples_before);

        // Write to file
        //fwrite(samples_buffer, 1, sizeof(samples_buffer), fp);
        fwrite(samples_buffer, 1, downsampler_instance.samples-samples_before, fp);
        
        free(samples);

        pos += to_read;
    }
    fclose(fp);

    return 0;
}