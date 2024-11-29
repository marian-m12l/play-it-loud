#include "gb_gfx.h"

#define TILE_WIDTH 8
#define TILE_HEIGHT 8

void convert_image_2bpp(uint16_t* image_data_rgb565, int width, int height, uint8_t* buffer) {

    unsigned char* pTileDataCurr = (unsigned char*) buffer;

    for (int tileY = 0; tileY < height; tileY += TILE_HEIGHT) {
        for (int tileX = 0; tileX < width; tileX += TILE_WIDTH) {
            for (int y = 0; y < TILE_HEIGHT; ++y) {
                for (int x = 0; x < TILE_WIDTH; x += TILE_WIDTH) {

                    unsigned char paletteIndices[TILE_WIDTH];
                    unsigned char topByte = 0;
                    unsigned char bottomByte = 0;

                    // Process a row of pixels and store their palette indices.
                    for (int i = 0; i < TILE_WIDTH; ++i) {
                        int index = (tileY + y) * width + (tileX + x + i);

                        float r = (float)((image_data_rgb565[index] >> 11) / 32.0f);
                        float g = (float)(((image_data_rgb565[index] >> 5) & 0x3f) / 64.0f);
                        float b = (float)((image_data_rgb565[index] & 0x1f) / 32.0f);

                        // Calculate luminance.
                        float lum = r * 0.3f + g * 0.59f + b * 0.11f;

                        // Map luminance to palette index. We'll assume palette goes 0 = "white" to 3 = "black"
                        unsigned char paletteIndex = 0;
                        if (lum < 0.33f) {
                            if (lum < 0.25f) {
                                paletteIndex = 3;
                            } else {
                                paletteIndex = 2;
                            }
                        } else if (lum < 0.66f) {
                            if (lum < 0.50f) {
                                paletteIndex = 2;
                            } else {
                                paletteIndex = 1;
                            }
                        } else if (lum <= 1.0f) {
                            if (lum < 0.76f) {
                                paletteIndex = 1;
                            } else {
                                paletteIndex = 0;
                            }
                        } else {
                            paletteIndex = 0;
                        }

                        paletteIndices[(TILE_WIDTH - 1) - i] = paletteIndex;
                    }

                    // Encode palette index to DMG tile format.
                    // 2 bytes make 1 horizontal line of a 8x8 tile

                    // Top Byte
                    for (int i = 0; i < TILE_WIDTH; ++i) {
                        unsigned char topBit = ((paletteIndices[i] >> 1) & 1) << i;
                        topByte = (topByte | topBit);
                    }

                    // Bottom Byte
                    for (int i = 0; i < TILE_WIDTH; ++i) {
                        unsigned char bottomBit = ((paletteIndices[i]) & 1) << i;
                        bottomByte = (bottomByte | bottomBit);
                    }

                    (*pTileDataCurr++) = topByte;
                    (*pTileDataCurr++) = bottomByte;
                }
            }
        }
    }
}
