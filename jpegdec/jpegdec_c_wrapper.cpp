#include "jpegdec_c_wrapper.h"
#include "JPEGDEC.h"

// 100x100 pixels image is centered onto a 112*104 pixels canvas, with a 2-pixel black border
#define CANVAS_WIDTH 112
#define IMAGE_WIDTH 100

static int jpeg_draw_cb(JPEGDRAW *pDraw) {
    // AVRCP cover thumbnails provide slices of 8 complete lines of 104 pixels
    uint16_t* decoded = (uint16_t*) pDraw->pUser;
    uint16_t* pixels = pDraw->pPixels;
    for (int y=0; y<pDraw->iHeight; y++) {
        // Leave the first and last 2 rows black (this makes the top and bottom 2-pixel borders)
        int y_offset = (pDraw->y + y + 2) * CANVAS_WIDTH;

        // Left border: 4 white + 2 black
        memset(decoded + y_offset + pDraw->x, 0xff, 4 * sizeof(uint16_t));
        memset(decoded + y_offset + pDraw->x + 4, 0, 2 * sizeof(uint16_t));
        // Centered image
        memcpy(decoded + y_offset + pDraw->x + 6, pixels, IMAGE_WIDTH * sizeof(uint16_t));
        // Right border: 2 black + 4 white
        memset(decoded + y_offset + pDraw->x + 106, 0, 2 * sizeof(uint16_t));
        memset(decoded + y_offset + pDraw->x + 108, 0xff, 4 * sizeof(uint16_t));

        pixels += pDraw->iWidth;
    }
    return 1;
}
 
void decode_jpeg(uint8_t* jpeg_data, int jpeg_filesize, uint16_t* decoded_buffer) {
    JPEGDEC decoder;
    decoder.openRAM(jpeg_data, jpeg_filesize, jpeg_draw_cb);
    decoder.setUserPointer(decoded_buffer);
    decoder.setPixelType(RGB565_LITTLE_ENDIAN);
    decoder.decode(0, 0, JPEG_SCALE_HALF);
    decoder.close();

    // Add top and bottom borders
    for (int y=0; y<2; y++) {
        memset(decoded_buffer + y * CANVAS_WIDTH, 0xff, 4 * sizeof(uint16_t));
        memset(decoded_buffer + y * CANVAS_WIDTH + 4, 0, 104 * sizeof(uint16_t));
        memset(decoded_buffer + y * CANVAS_WIDTH + 108, 0xff, 4 * sizeof(uint16_t));
    }
    for (int y=102; y<104; y++) {
        memset(decoded_buffer + y * CANVAS_WIDTH, 0xff, 4 * sizeof(uint16_t));
        memset(decoded_buffer + y * CANVAS_WIDTH + 4, 0, 104 * sizeof(uint16_t));
        memset(decoded_buffer + y * CANVAS_WIDTH + 108, 0xff, 4 * sizeof(uint16_t));
    }
}
