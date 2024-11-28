#ifndef avrcp_h
#define avrcp_h

#include <btstack.h>

void avrcp_begin();

uint8_t avrcp_get_volume();  // 0..127
bool avrcp_is_connected();
bool avrcp_is_playing();

// FIXME Need a better API to pause on track change
bool avrcp_track_changed();
void avrcp_pause();

bool avrcp_cover_loaded();
uint8_t* avrcp_cover_tiles();
char* avrcp_get_artist();
char* avrcp_get_track();

#endif