#pragma once

#include <stdint.h>

void convert_image_2bpp(uint16_t* image_data_rgb565, int width, int height, uint8_t* buffer);
