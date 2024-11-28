#include "jpegdec_c_wrapper.h"
#include "JPEGDEC.h"

static int jpeg_draw_cb(JPEGDRAW *pDraw) {
    int image_width = 100;  // TODO Ignore additional pixels ?
    uint16_t* decoded = (uint16_t*) pDraw->pUser;
    uint16_t* pixels = pDraw->pPixels;
    // TODO Receiving slices of 8 complete lines of 104 pixels !!
    //printf("x=%d y=%d w=%d h=%d used=%d offset=%d\n", pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight, pDraw->iWidthUsed, (pDraw->y*image_width)+pDraw->x);
    for (int y=0; y<pDraw->iHeight; y++) {
        //printf("(%d,%d) dest=%p pPixels=%p\n", pDraw->y+y, pDraw->x, decoded+((pDraw->y+y)*image_width)+pDraw->x, pixels);

        // TODO convert to 112*112 with 1 ou 2px black border and white around the centered image (to make it 14x14 tiles on GB)

        *(decoded+((pDraw->y+y+2)*pDraw->iWidth)+pDraw->x) = 0;
        *(decoded+((pDraw->y+y+2)*pDraw->iWidth)+pDraw->x+1) = 0;
        memcpy(decoded+((pDraw->y+y+2)*pDraw->iWidth)+pDraw->x+2, pixels, image_width*sizeof(uint16_t));
        *(decoded+((pDraw->y+y+2)*pDraw->iWidth)+pDraw->x+102) = 0;
        *(decoded+((pDraw->y+y+2)*pDraw->iWidth)+pDraw->x+103) = 0;
        pixels += pDraw->iWidth;
    }
    //printf("pPixels=%p\n", pixels);
    // TODO DEBUG: Print all pPixels as-is
    /*for (int y=0; y<pDraw->iHeight; y++) {
        for (int x=0; x<pDraw->iWidth; x++) {
        printf("%04x ", *(pixels++));
        }
        printf("\n");
    }*/
    return 1;
}
 
void decode_jpeg(uint8_t* jpeg_data, int jpeg_filesize, uint16_t* decoded_buffer) {
    JPEGDEC decoder;
    decoder.openRAM(jpeg_data, jpeg_filesize, jpeg_draw_cb);
    decoder.setUserPointer(decoded_buffer);
    decoder.setPixelType(RGB565_LITTLE_ENDIAN);
    decoder.decode(0, 0, JPEG_SCALE_HALF);
    decoder.close();
}
