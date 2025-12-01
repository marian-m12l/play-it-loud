
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

const unsigned char mvols[] = {7, 6, 7, 6, 7, 5, 6, 7, 5, 6, 4, 5, 6, 4, 5, 7, 4, 6, 5, 4, 2, 7, 6, 5, 4, 3, 2, 1, 0, 0};
const unsigned char pulsel[] = {0, 0, 1, 1, 2, 1, 2, 3, 2, 3, 2, 3, 4, 3, 4, 5, 4, 5, 5, 5, 4, 6, 6, 6, 6, 6, 6, 6, 6, 7};
const unsigned char pulseh[] = {15, 15, 14, 14, 13, 14, 13, 12, 13, 12, 13, 12, 11, 12, 11, 10, 11, 10, 10, 10, 11, 9, 9, 9, 9, 9, 9, 9, 9, 8};

typedef struct {
    unsigned char min;
    unsigned char max;
} lut_entry_t;

const lut_entry_t LUT[] = {
    {0, 14}, {15, 17}, {18, 30}, {31, 35}, {36, 44},
    {45, 46}, {47, 53}, {54, 57}, {58, 62}, {63, 69},
    {70, 71}, {72, 78}, {79, 80}, {81, 85}, {86, 89},
    {90, 91}, {92, 94}, {95, 98}, {99, 103}, {104, 105},
    {106, 107}, {108, 110}, {111, 112}, {113, 114}, {115, 116},
    {117, 119}, {120, 121}, {122, 123}, {124, 126}, {127, 127},
    {128, 128}, {129, 130}, {131, 132}, {133, 134}, {135, 137},
    {138, 139}, {140, 141}, {142, 143}, {144, 146}, {147, 148},
    {149, 150}, {151, 155}, {156, 159}, {160, 162}, {163, 164},
    {165, 168}, {169, 173}, {174, 175}, {176, 182}, {183, 184},
    {185, 191}, {192, 196}, {197, 200}, {201, 207}, {208, 211},
    {210, 218}, {219, 223}, {224, 236}, {237, 239}, {240, 255}
};

//this method gives the config for a fixed set of values,
//since this is the best LUT so far.
unsigned char quantize(unsigned char sample) {
    unsigned char label = 0;
    while (sample < LUT[label].min || sample > LUT[label].max) {
        label++;
    }

    printf("label=%d\n", label);

    unsigned char config = label < 30
        ? (pulsel[label] << 4) | mvols[label]
        : (pulseh[59-label] << 4) | mvols[59-label];
    return config;
}

unsigned char decode(unsigned char encoded) {
    unsigned char vol = encoded & 0x7;
    unsigned char pulse = encoded >> 4;

    unsigned char label = 0;
    while (!(pulse == pulsel[label] && vol == mvols[label]) && label < 30) {
        label++;
    }
    while (!(pulse == pulseh[59-label] && vol == mvols[59-label]) && label >= 30 && label < 60) {
        label++;
    }

    //printf("label=%d\n", label);
    
    return label == 60 ? 0xff : (LUT[label].min + LUT[label].max) / 2;
}

int s_gets(char* str, int n)
{
    char* str_read = fgets(str, n, stdin);
    if (!str_read)
        return -1;

    int i = 0;
    while (str[i] != '\n' && str[i] != '\0')
        i++;

    if (str[i] == '\n')
        str[i] = '\0';

    return 0;
}

int main() {
    for (int sample=0x00; sample <= 0xff; sample++) {
        printf("quantize(0x%02x) = 0x%02x\n", sample, quantize(sample));
    }
    
    for (int encoded=0x00; encoded <= 0xff; encoded++) {
        printf("decoded(0x%02x) = 0x%02x\n", encoded, decode(encoded));
    }

    /*int samples = 0;
    char cell[6];
    unsigned int encoded;
    while (s_gets(cell, 6) == 0) {
        //printf("cell = %s\n", cell);
        sscanf(cell, "%x", &encoded);
        //printf("encoded = 0x%02X\n", encoded);
        printf("#%06d: 0x%02x -> 0x%02x\n", samples, encoded, decode(encoded));
        samples++;
    }
    printf("Encoded samples: %d\n", samples);
    */

    uint32_t duration = 10;
    uint32_t rate = 16384;  // Sample rate
    uint32_t frame_count = duration * rate;
    uint16_t chan_num = 1;  // Number of channels
    uint16_t bits = 16;  // Bit depth
    uint32_t length = frame_count*chan_num*bits / 8;
    int16_t byte;
    float sync_level = 1.0;
    // scales signal to use full 16bit resolution
    float multiplier = 32767 / sync_level;

    float *channel1 = (float *) malloc(frame_count * sizeof(float));
    for (uint32_t i=0; i < frame_count; i++) {
        channel1[i] = (float) (rand() - RAND_MAX / 2) / RAND_MAX;
    }

    // Writes data to wav file
    FILE *fp = fopen("decoded_from_csv.wav", "w");
    if (fp == NULL) {
        printf("Output file couldn't be opened!\n");
        return 4;
    }

    //// WAVE Header Data
    fwrite("RIFF", 1, 4, fp);
    uint32_t chunk_size = length + 44 - 8;
    fwrite(&chunk_size, 4, 1, fp);
    fwrite("WAVE", 1, 4, fp);
    fwrite("fmt ", 1, 4, fp);
    uint32_t subchunk1_size = 16;
    fwrite(&subchunk1_size, 4, 1, fp);
    uint16_t fmt_type = 1;  // 1 = PCM
    fwrite(&fmt_type, 2, 1, fp);
    fwrite(&chan_num, 2, 1, fp);
    fwrite(&rate, 4, 1, fp);
    // (Sample Rate * BitsPerSample * Channels) / 8
    uint32_t byte_rate = rate * bits * chan_num / 8;
    fwrite(&byte_rate, 4, 1, fp);
    uint16_t block_align = chan_num * bits / 8;
    fwrite(&block_align, 2, 1, fp);
    fwrite(&bits, 2, 1, fp);

    // Marks the start of the data
    fwrite("data", 1, 4, fp);
    fwrite(&length, 4, 1, fp);  // Data size
    for (uint32_t i = 0; i < frame_count; i++) {
        byte = (channel1[i] * multiplier);
        char cell[6];
        if ((s_gets(cell, 6) == 0)) {
            unsigned int encoded;
            sscanf(cell, "%x", &encoded);
            byte = (decode(encoded) << 8) - 32768;
        }
        fwrite(&byte, 2, 1, fp);
    }

    fclose(fp);
    free(channel1);

    return 0;
}