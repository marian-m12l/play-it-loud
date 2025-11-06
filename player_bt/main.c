#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pico/cyw43_arch.h"

#include "gb_serial.h"
#include "gb_audio.h"
#include "bt.h"
#include "cover.h"

// Declare pins in binary info
bi_decl(bi_3pins_with_names(
    PIN_SERIAL_CLOCK, "GB Serial Clock",
    PIN_SERIAL_MISO, "GB Serial IN",
    PIN_SERIAL_MOSI, "GB Serial OUT"
));

char artist_bt[] = "Bluetooth";
char title_bt[] = "A2DP / AVRCP Sink";

void on_bt_up( void * ) {
    printf("Bluetooth stack is up\n");
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
}


int main() {
    stdio_init_all();

    sleep_ms(1000);

    printf("Bluetooth A2DP Sink Player\n");

#if ENABLE_DOUBLE_SPEED == 1
    printf("Playback rate: 16384Hz\n");
#else
    printf("Playback rate: 8192Hz\n");
#endif

    if (cyw43_arch_init()) {
        printf("Failed to init cyw43_arch\n");
        return -1;
    }

    // LED ON during setup until bt is up
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);

    bt_begin(BT_NAME, BT_PIN, on_bt_up, NULL);

    printf("Setup done\n");

    // Send default cover on boot
    gb_serial_init();
    gb_audio_new_track_blocking(cover_tiles_bt, artist_bt, title_bt);

    bt_run();
}