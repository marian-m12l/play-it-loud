// Original work by joba-1: https://github.com/joba-1/PicoW_A2DP

#include "avrcp.h"
#include <stdio.h>
#include <inttypes.h>

#include "jpegdec_c_wrapper.h"
#include "gb_gfx.h"
#include "gb_audio.h"

static uint16_t _cid = 0;
static bool _playing = false;
static uint8_t _volume = 0;

// Cover art
bd_addr_t remote_address;

static char a2dp_sink_demo_image_handle[8];
static avrcp_cover_art_client_t a2dp_sink_demo_cover_art_client;
static bool a2dp_sink_demo_cover_art_client_connected;
static uint16_t a2dp_sink_demo_cover_art_cid;
static uint8_t a2dp_sink_demo_ertm_buffer[2000];
static l2cap_ertm_config_t a2dp_sink_demo_ertm_config = {
        1,  // ertm mandatory
        2,  // max transmit, some tests require > 1
        2000,
        12000,
        512,    // l2cap ertm mtu
        2,
        2,
        1,      // 16-bit FCS
};
static bool a2dp_sink_cover_art_download_active;
static uint32_t a2dp_sink_cover_art_file_size;
static uint8_t cover_art_jpeg[60000];

uint8_t cover_tiles[14*13*16];
char artist[19];
char title[19];

#define TRACK_POLL_INTERVAL_MS   5

// timer to send new track cover and metadata
static btstack_timer_source_t  track_timer_sink;

bool trigger_started_tx = false;
bool cover_started_tx = false;
bool metadata_started_tx = false;
bool ready_to_stream = false;

static void timer_handler_cover(btstack_timer_source_t * ts) {
    if (!trigger_started_tx) {
        gb_audio_new_track_step1();
        trigger_started_tx = true;
    } else if (gb_audio_new_track_step1_done()) {
        if (!cover_started_tx) {
            gb_audio_new_track_step2(cover_tiles);
            cover_started_tx = true;
        } else if (gb_audio_new_track_step2_done()) {
            if (!metadata_started_tx) {
                gb_audio_new_track_step3(artist, title);
                metadata_started_tx = true;
            } else if (gb_audio_new_track_step3_done()) {
                printf("AVRCP is ready for audio samples streaming\n");
                ready_to_stream = true;
                if (_playing) {
                    printf("AVRCP Start streaming audio samples\n");
                    const btstack_audio_sink_t * audio = btstack_audio_sink_get_instance();
                    if (audio) {
                        audio->start_stream();
                    }
                }
                return;
            }
        }
    }

    btstack_run_loop_set_timer(ts, TRACK_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(ts);
}


static void a2dp_sink_demo_cover_art_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);
    uint8_t status;
    uint16_t cid;
    switch (packet_type){
        case BIP_DATA_PACKET:
            if (a2dp_sink_cover_art_download_active){
                if (a2dp_sink_cover_art_file_size + size < sizeof(cover_art_jpeg)){
                    memcpy(&cover_art_jpeg[a2dp_sink_cover_art_file_size], packet, size);
                }
                a2dp_sink_cover_art_file_size += size;
            }
            break;
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)){
                case HCI_EVENT_AVRCP_META:
                    switch (hci_event_avrcp_meta_get_subevent_code(packet)){
                        case AVRCP_SUBEVENT_COVER_ART_CONNECTION_ESTABLISHED:
                            status = avrcp_subevent_cover_art_connection_established_get_status(packet);
                            cid = avrcp_subevent_cover_art_connection_established_get_cover_art_cid(packet);
                            if (status == ERROR_CODE_SUCCESS){
                                printf("Cover Art       : connection established, cover art cid 0x%02x\n", cid);
                                a2dp_sink_demo_cover_art_client_connected = true;
                            } else {
                                printf("Cover Art       : connection failed, status 0x%02x\n", status);
                                a2dp_sink_demo_cover_art_cid = 0;
                            }
                            break;
                        
                        case AVRCP_SUBEVENT_COVER_ART_OPERATION_COMPLETE:
                            if (a2dp_sink_cover_art_download_active){
                                a2dp_sink_cover_art_download_active = false;
                                printf("Cover Art: download completed (%d bytes)\n", a2dp_sink_cover_art_file_size);
                                
                                // We need to decode thumbnail jpeg (200x200) with JPEG_SCALE_HALF (100x100) and store it in a tiles-aligned canvas
                                uint16_t* decoded = malloc(112 * 104 * sizeof(uint16_t));
                                decode_jpeg(cover_art_jpeg, a2dp_sink_cover_art_file_size, decoded);
                                convert_image_2bpp(decoded, 112, 104, cover_tiles);
                                free(decoded);
                                printf("Cover image converted to GB tiles\n");

                                // start timer to send cover and metadata to the GB
                                btstack_run_loop_set_timer_handler(&track_timer_sink, &timer_handler_cover);
                                btstack_run_loop_set_timer(&track_timer_sink, TRACK_POLL_INTERVAL_MS);
                                btstack_run_loop_add_timer(&track_timer_sink);
                            }
                            break;
                        case AVRCP_SUBEVENT_COVER_ART_CONNECTION_RELEASED:
                            a2dp_sink_demo_cover_art_client_connected = false;
                            a2dp_sink_demo_cover_art_cid = 0;
                            printf("Cover Art       : connection released 0x%02x\n",
                                   avrcp_subevent_cover_art_connection_released_get_cover_art_cid(packet));
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

static uint8_t a2dp_sink_demo_cover_art_connect(void) {
    return avrcp_cover_art_client_connect(&a2dp_sink_demo_cover_art_client, a2dp_sink_demo_cover_art_packet_handler,
                                          remote_address, a2dp_sink_demo_ertm_buffer,
                                          sizeof(a2dp_sink_demo_ertm_buffer), &a2dp_sink_demo_ertm_config,
                                          &a2dp_sink_demo_cover_art_cid);
}



static void avrcp_volume_changed(uint8_t volume){
    const btstack_audio_sink_t * audio = btstack_audio_sink_get_instance();
    if (audio){
        audio->set_volume(volume);
    }
}

static void connection_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);

    uint16_t cid;
    uint8_t  status;

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_AVRCP_META) return;

    switch (packet[2]) {
        case AVRCP_SUBEVENT_CONNECTION_ESTABLISHED:
            cid = avrcp_subevent_connection_established_get_avrcp_cid(packet);
            status = avrcp_subevent_connection_established_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                printf("AVRCP: Connection failed, status 0x%02x\n", status);
                _cid = 0;
                return;
            }

            _cid = cid;

            // Store remote device address
            avrcp_subevent_connection_established_get_bd_addr(packet, remote_address);
            printf("AVRCP: Connected to %s, cid 0x%02x\n", bd_addr_to_str(remote_address), _cid);

            avrcp_target_support_event(cid, AVRCP_NOTIFICATION_EVENT_VOLUME_CHANGED);
            avrcp_target_support_event(cid, AVRCP_NOTIFICATION_EVENT_BATT_STATUS_CHANGED);
            avrcp_target_battery_status_changed(cid, AVRCP_BATTERY_STATUS_EXTERNAL);
        
            // query supported events:
            avrcp_controller_get_supported_events(cid);
            return;
        
        case AVRCP_SUBEVENT_CONNECTION_RELEASED:
            _cid = 0;
            return;

        default:
            break;
    }
}


static void controller_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);

    uint8_t play_status;
    uint8_t avrcp_subevent_value[256];
    uint8_t event_id;
    uint8_t status;

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_AVRCP_META) return;
    if (_cid == 0) return;

    memset(avrcp_subevent_value, 0, sizeof(avrcp_subevent_value));
    switch (packet[2]) {

        case AVRCP_SUBEVENT_GET_CAPABILITY_EVENT_ID_DONE:

            // automatically enable notifications
            avrcp_controller_enable_notification(_cid, AVRCP_NOTIFICATION_EVENT_PLAYBACK_STATUS_CHANGED);
            avrcp_controller_enable_notification(_cid, AVRCP_NOTIFICATION_EVENT_NOW_PLAYING_CONTENT_CHANGED);
            avrcp_controller_enable_notification(_cid, AVRCP_NOTIFICATION_EVENT_TRACK_CHANGED);
            // image handles become invalid on player change, register for notifications
            avrcp_controller_enable_notification(_cid, AVRCP_NOTIFICATION_EVENT_UIDS_CHANGED);
            status = a2dp_sink_demo_cover_art_connect();
            printf("Cover Art: connect -> status %02x\n", status);
            break;

        case AVRCP_SUBEVENT_NOTIFICATION_STATE:
            event_id = (avrcp_notification_event_id_t)avrcp_subevent_notification_state_get_event_id(packet);
            printf("AVRCP Controller: %s notification registered\n", avrcp_notification2str((avrcp_notification_event_id_t)event_id));
            break;

        case AVRCP_SUBEVENT_NOTIFICATION_PLAYBACK_POS_CHANGED:
            printf("AVRCP Controller: Playback position changed, position %d ms\n", (unsigned int) avrcp_subevent_notification_playback_pos_changed_get_playback_position_ms(packet));
            break;

        case AVRCP_SUBEVENT_NOTIFICATION_PLAYBACK_STATUS_CHANGED:
            printf("AVRCP Controller: Playback status changed %s\n", avrcp_play_status2str(avrcp_subevent_notification_playback_status_changed_get_play_status(packet)));
            play_status = avrcp_subevent_notification_playback_status_changed_get_play_status(packet);
            switch (play_status){
                case AVRCP_PLAYBACK_STATUS_PLAYING:
                    _playing = true;
                    if (ready_to_stream) {
                        printf("AVRCP Start streaming audio samples\n");
                        const btstack_audio_sink_t * audio = btstack_audio_sink_get_instance();
                        if (audio) {
                            audio->start_stream();
                        }
                    }
                    break;
                default:
                    _playing = false;
                    printf("AVRCP Stop streaming audio samples\n");
                    const btstack_audio_sink_t * audio = btstack_audio_sink_get_instance();
                    if (audio) {
                        audio->stop_stream();
                    }
                    break;
            }
            break;

        case AVRCP_SUBEVENT_NOTIFICATION_NOW_PLAYING_CONTENT_CHANGED:
            printf("AVRCP Controller: Playing content changed\n");
            break;

        case AVRCP_SUBEVENT_NOTIFICATION_TRACK_CHANGED:
            printf("AVRCP Controller: Track changed\n");

            trigger_started_tx = false;
            cover_started_tx = false;
            metadata_started_tx = false;
            ready_to_stream = false;

            printf("AVRCP Stop streaming audio samples\n");
            const btstack_audio_sink_t * audio = btstack_audio_sink_get_instance();
            if (audio) {
                audio->stop_stream();
            }

            // Fetch info (and then cover)
            avrcp_controller_get_now_playing_info(_cid);
            break;

        case AVRCP_SUBEVENT_SHUFFLE_AND_REPEAT_MODE:{
            uint8_t shuffle_mode = avrcp_subevent_shuffle_and_repeat_mode_get_shuffle_mode(packet);
            uint8_t repeat_mode  = avrcp_subevent_shuffle_and_repeat_mode_get_repeat_mode(packet);
            printf("AVRCP Controller: %s, %s\n", avrcp_shuffle2str(shuffle_mode), avrcp_repeat2str(repeat_mode));
            break;
        }
        case AVRCP_SUBEVENT_NOW_PLAYING_TRACK_INFO:
            printf("AVRCP Controller: Track %d\n", avrcp_subevent_now_playing_track_info_get_track(packet));
            break;

        case AVRCP_SUBEVENT_NOW_PLAYING_TITLE_INFO:
            if (avrcp_subevent_now_playing_title_info_get_value_len(packet) > 0){
                memcpy(avrcp_subevent_value, avrcp_subevent_now_playing_title_info_get_value(packet), avrcp_subevent_now_playing_title_info_get_value_len(packet));
                printf("AVRCP Controller: Title %s\n", avrcp_subevent_value);
                int len = avrcp_subevent_now_playing_title_info_get_value_len(packet);
                len = len > sizeof(title) ? sizeof(title) : len;
                memcpy(title, avrcp_subevent_value, len);
                title[len] = 0;
            }  
            break;

        case AVRCP_SUBEVENT_NOW_PLAYING_ARTIST_INFO:
            if (avrcp_subevent_now_playing_artist_info_get_value_len(packet) > 0){
                memcpy(avrcp_subevent_value, avrcp_subevent_now_playing_artist_info_get_value(packet), avrcp_subevent_now_playing_artist_info_get_value_len(packet));
                printf("AVRCP Controller: Artist %s\n", avrcp_subevent_value);
                int len = avrcp_subevent_now_playing_title_info_get_value_len(packet);
                len = len > sizeof(artist) ? sizeof(artist) : len;
                memcpy(artist, avrcp_subevent_value, len);
                artist[len] = 0;
            }  
            break;
        
        case AVRCP_SUBEVENT_NOW_PLAYING_ALBUM_INFO:
            if (avrcp_subevent_now_playing_album_info_get_value_len(packet) > 0){
                memcpy(avrcp_subevent_value, avrcp_subevent_now_playing_album_info_get_value(packet), avrcp_subevent_now_playing_album_info_get_value_len(packet));
                printf("AVRCP Controller: Album %s\n", avrcp_subevent_value);
            }  
            break;

        case AVRCP_SUBEVENT_PLAY_STATUS:
            printf("AVRCP Controller: Song length %"PRIu32" ms, Song position %"PRIu32" ms, Play status %s\n", 
            avrcp_subevent_play_status_get_song_length(packet), 
            avrcp_subevent_play_status_get_song_position(packet),
            avrcp_play_status2str(avrcp_subevent_play_status_get_play_status(packet)));
            break;
        
        case AVRCP_SUBEVENT_NOTIFICATION_EVENT_UIDS_CHANGED:
            if (a2dp_sink_demo_cover_art_client_connected){
                printf("AVRCP Controller: UIDs changed -> disconnect cover art client\n");
                avrcp_cover_art_client_disconnect(a2dp_sink_demo_cover_art_cid);
            }
            break;

        case AVRCP_SUBEVENT_NOW_PLAYING_COVER_ART_INFO:
            if (avrcp_subevent_now_playing_cover_art_info_get_value_len(packet) == 7){
                memcpy(a2dp_sink_demo_image_handle, avrcp_subevent_now_playing_cover_art_info_get_value(packet), 7);
                printf("AVRCP Controller: Cover Art %s\n", a2dp_sink_demo_image_handle);
                if (a2dp_sink_cover_art_download_active == false){
                    a2dp_sink_cover_art_download_active = true;
                    a2dp_sink_cover_art_file_size = 0;
                    status = avrcp_cover_art_client_get_linked_thumbnail(a2dp_sink_demo_cover_art_cid, a2dp_sink_demo_image_handle);
                    if (status != ERROR_CODE_SUCCESS){
                        a2dp_sink_cover_art_download_active = false;
                    }
                    printf("Cover Art       : download thumbnail %s started\n", a2dp_sink_demo_image_handle);
                }
            }
            break;

        default:
            break;
    }
}


static void target_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_AVRCP_META) return;
    
    switch (packet[2]){
        case AVRCP_SUBEVENT_NOTIFICATION_VOLUME_CHANGED:
            _volume = avrcp_subevent_notification_volume_changed_get_absolute_volume(packet);
            avrcp_volume_changed(_volume);
            break;

        default:
            break;
    }
}


void avrcp_begin() {
    goep_client_init();
    avrcp_cover_art_client_init();

    avrcp_init();
    avrcp_controller_init();
    avrcp_target_init();

    avrcp_register_packet_handler(connection_handler);
    avrcp_controller_register_packet_handler(controller_handler);
    avrcp_target_register_packet_handler(target_handler);
}


uint8_t avrcp_get_volume() { 
    return _volume; 
};


bool avrcp_is_connected() { 
    return _cid != 0; 
};


bool avrcp_is_playing() { 
    return _playing; 
};

bool avrcp_is_ready() { 
    return ready_to_stream; 
};
