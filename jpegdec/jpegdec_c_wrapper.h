#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
 
void decode_jpeg(uint8_t* jpeg_data, int jpeg_filesize, uint16_t* decoded_buffer);

#ifdef __cplusplus
}
#endif
