// Original work by jbshelton: https://github.com/jbshelton/GBAudioPlayer

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "encoder.h"

const unsigned char mvols[] = {7, 6, 7, 6, 7, 5, 6, 7, 5, 6, 4, 5, 6, 4, 5, 7, 4, 6, 5, 4, 2, 7, 6, 5, 4, 3, 2, 1, 0, 0};
const unsigned char pulsel[] = {0, 0, 1, 1, 2, 1, 2, 3, 2, 3, 2, 3, 4, 3, 4, 5, 4, 5, 5, 5, 4, 6, 6, 6, 6, 6, 6, 6, 6, 7};
const unsigned char pulseh[] = {15, 15, 14, 14, 13, 14, 13, 12, 13, 12, 13, 12, 11, 12, 11, 10, 11, 10, 10, 10, 11, 9, 9, 9, 9, 9, 9, 9, 9, 8};

typedef struct {
    unsigned char min;
    unsigned char max;
} lut_entry_t;

const lut_entry_t LUT[] = {
    {0, 15}, {15, 18}, {18, 31}, {31, 36}, {36, 45},
    {45, 47}, {47, 54}, {54, 58}, {58, 63}, {63, 70},
    {70, 72}, {72, 79}, {79, 81}, {81, 86}, {86, 90},
    {90, 92}, {92, 95}, {95, 99}, {99, 104}, {104, 106},
    {106, 108}, {108, 111}, {111, 113}, {113, 115}, {115, 117},
    {117, 120}, {120, 122}, {122, 124}, {124, 127}, {127, 128},
    {128, 129}, {129, 131}, {131, 133}, {133, 135}, {135, 138},
    {138, 140}, {140, 142}, {142, 144}, {144, 147}, {147, 149},
    {149, 151}, {151, 156}, {156, 160}, {160, 163}, {163, 165},
    {165, 169}, {169, 174}, {174, 176}, {176, 183}, {183, 185},
    {185, 192}, {192, 197}, {197, 201}, {201, 208}, {208, 210},
    {210, 219}, {219, 224}, {224, 237}, {237, 240}, {240, 255}
};

//this method gives the config for a fixed set of values,
//since this is the best LUT so far.
unsigned char quantize(unsigned char sample) {
    unsigned char label = 0;
    while (sample < LUT[label].min || sample >= LUT[label].max) {
        label++;
    }
    unsigned char config = label < 30
        ? (pulsel[label] << 4) | mvols[label]
        : (pulseh[59-label] << 4) | mvols[59-label];
    return config;
}

char sign(char x) {
    return (x > 0) - (x < 0);
}

//this first check method checks to see if the target config is high priority
//(no problems with triggering pulse first)
bool check1(unsigned char lastconfig, unsigned char thisconfig)
{
    unsigned char lastpulse = lastconfig >> 4;
    unsigned char thispulse = thisconfig >> 4;
    unsigned char lastvol = lastconfig & 7;
    unsigned char thisvol = thisconfig & 7;

    // Same pulse or same volume
    if ((lastpulse == thispulse) || (lastvol == thisvol)) {
        return true;
    }
    // Both pulses high
    if ((thispulse > 7) && (lastpulse > 7)) {
        return sign(thispulse - lastpulse) == sign(thisvol - lastvol);
    }
    // Both pulses low
    if ((thispulse < 8) && (lastpulse < 8)) {
        return sign(thispulse - lastpulse) == sign(lastvol - thisvol);
    }
    // Pulse transitioning
    if (((thispulse > 7) && (lastpulse < 8)) || (thispulse < 8) && (lastpulse > 7)) {
        return thisvol > lastvol;
    }
    // Otherwise
    return false;
}

//this second check method evaluates the best option
//for when pulse and master volume are moving in different
//directions.
//return 0 or 2 is for use pulse first, 1 or 3 is for vol first,
//and >=2 is for enabling a warning.
unsigned char check6(unsigned char currtemp, unsigned char prevtemp, unsigned char threshold) {
    // TODO Separate condition for bit 0 and bit 1 ??
    if (currtemp < prevtemp)    return (currtemp <= threshold) ? 0 : 2;
    else                        return (prevtemp <= threshold) ? 1 : 3;
}
unsigned char check5(unsigned char currtemp, unsigned char prevtemp, unsigned char left8bit, unsigned char right8bit) {
    if (currtemp > left8bit)       return (prevtemp > left8bit) ? check6(currtemp, prevtemp, left8bit+16) : 1;
    if (currtemp > right8bit)      return (prevtemp > right8bit) ? check6(currtemp, prevtemp, right8bit-16) : 1;
    return 0;
}
unsigned char check4(unsigned char currtemp, unsigned char prevtemp, unsigned char left8bit, unsigned char right8bit) {
    if((currtemp <= left8bit) && (currtemp >= right8bit)) {
        return 0;
    }
    if((prevtemp <= left8bit) && (prevtemp >= right8bit)) {
        return 1;
    }
    return check5(currtemp, prevtemp, left8bit, right8bit);
}
unsigned char check3(unsigned char currtemp, unsigned char prevtemp, unsigned char curr8bit, unsigned char prev8bit) {
    if(curr8bit > prev8bit) {
        return check4(currtemp, prevtemp, curr8bit, prev8bit);
    } else {
        return check4(currtemp, prevtemp, prev8bit, curr8bit);
    }
}
unsigned char check2(unsigned char lastconfig, unsigned char thisconfig) 
{
    unsigned char lastpulse = lastconfig >> 4;
    unsigned char thispulse = thisconfig >> 4;
    unsigned char lastvol = lastconfig & 7;
    unsigned char thisvol = thisconfig & 7;
    unsigned char prevtemp = 0; //load master volume first
    unsigned char currtemp = 0; //trigger pulse first
    unsigned char prev8bit = 0;
    unsigned char curr8bit = 0;

    unsigned char mid = (thispulse<8) ? 127 : 128;
    char temp = (thispulse<8) ? thispulse-7 : thispulse-8;   // map pulse to a 0-7 range (negative or positive)
    curr8bit = (unsigned char)(mid+(127*((temp/7)*((thisvol+1)/8))));
    currtemp = (unsigned char)(mid+(127*((temp/7)*((lastvol+1)/8))));
    temp = (lastpulse<8) ? lastpulse-7 : lastpulse-8;
    prev8bit = (unsigned char)(mid+(127*((temp/7)*((lastvol+1)/8))));
    prevtemp = (unsigned char)(mid+(127*((temp/7)*((thisvol+1)/8))));

    return check3(currtemp, prevtemp, curr8bit, prev8bit);
}

void encode_init(encoder_t* instance) {
    instance->samples = 0;
    instance->curr_config = 0;
    instance->prev_config = 0;
    instance->curr_sample = 0;
}

void encode(encoder_t* instance, const unsigned char input[], unsigned char * output, int length) {
    int i = 0;
    
    // First sample
    if (instance->samples == 0) {
        instance->prev_config = quantize(input[i]);
        output[i++] = instance->prev_config;
    }
    for(; i < length; i++) {
        instance->curr_sample = input[i];
        instance->curr_config = quantize(instance->curr_sample);
        if(check1(instance->prev_config, instance->curr_config)) {
            output[i] = instance->curr_config;
        } else {
            unsigned char c2 = check2(instance->prev_config, instance->curr_config);
            if((c2 & 0x01) != 0) {
                output[i] = instance->curr_config | 8;
            } else {
                output[i] = instance->curr_config;
            }
        }
        instance->prev_config = instance->curr_config;
    }
    instance->samples += length;
}
