#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pico/cyw43_arch.h"

#include "gb_serial.h"
#include "bt.h"

// Declare pins in binary info
bi_decl(bi_3pins_with_names(
    PIN_SERIAL_CLOCK, "GB Serial Clock",
    PIN_SERIAL_MISO, "GB Serial IN",
    PIN_SERIAL_MOSI, "GB Serial OUT"
));

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
    bt_run();
}