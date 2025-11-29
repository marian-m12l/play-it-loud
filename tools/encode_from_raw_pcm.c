
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "../gb_serial/encoder.h"

int main() {

    // Reads data from raw pcm file

    // Writes data to bin file
    FILE *fp = fopen("encoded_from_raw_pcm.bin", "w");
    if (fp == NULL) {
        printf("Output file couldn't be opened!\n");
        return 4;
    }

    #define BUFFER_SIZE 164
    #define EXPECTED_SAMPLES 164

    unsigned char samples_buffer[BUFFER_SIZE];
    
    encoder_t encoder_instance;

    encode_init(&encoder_instance);

    // Loop, read 10 ms of samples
    // FIXME file size ??
    int length = 990212;
    for (int pos=0; pos<length;) {
        int to_read = EXPECTED_SAMPLES;
        if (length - pos < to_read) {
            to_read = length - pos;
        }
        uint8_t *samples = malloc(to_read);
        fread(samples, to_read, 1, stdin);

        // Encode
        encode(&encoder_instance, samples, samples_buffer, to_read);

        // Write to file
        fwrite(samples_buffer, 1, sizeof(samples_buffer), fp);
        
        free(samples);

        pos += to_read;
    }
    fclose(fp);

    return 0;
}