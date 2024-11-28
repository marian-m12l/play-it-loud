#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "spi.pio.h"

#include "gb_serial.h"
#include "downsampler.h"
#include "encoder.h"

#define INPUT_SAMPLE_RATE 44100
#define DECIMATE_FACTOR 7
#define TARGET_SAMPLE_RATE (INPUT_SAMPLE_RATE/DECIMATE_FACTOR)
#define PLAYBACK_RATE (256*1024/42)
#define PLAYBACK_SAMPLE_DURATION_US (1000000/PLAYBACK_RATE)

// TODO Make buffer configurable at build time ??
#define BUFFER_SIZE 74
#define EXPECTED_SAMPLES (DECIMATE_FACTOR*BUFFER_SIZE)
unsigned char playback_buffer_0[BUFFER_SIZE];
unsigned char playback_buffer_1[BUFFER_SIZE];
unsigned char playback_buffer_2[BUFFER_SIZE];

unsigned char* filling_buffer = playback_buffer_0;
unsigned char* playback_buffer = playback_buffer_2;

downsampler_t downsampler_instance;
encoder_t encoder_instance;

int input;

int dma_channel_tx;
int dma_timer;
int dma_channel_rx;

PIO pio = pio0;
uint sm = 0;

bool streaming = false;


void swap_buffers(unsigned char** buffer) {
    if (*buffer == playback_buffer_0) {
        *buffer = playback_buffer_1;
    } else if (*buffer == playback_buffer_1) {
        *buffer = playback_buffer_2;
    } else {
        *buffer = playback_buffer_0;
    }
}

void __isr __time_critical_func(dma_handler)() {
    if (dma_channel_get_irq0_status(dma_channel_tx)) {
        // Clear the interrupt request.
        dma_hw->ints0 = 1u << dma_channel_tx;

        if (streaming) {
            // Swap buffers
            swap_buffers(&playback_buffer);

            // Give the channel a new buffer to read from, and re-trigger it
            //printf("PLAY buffer: 0x%p of %d bytes (from IRQ)\n", playback_buffer, BUFFER_SIZE);
            dma_channel_transfer_from_buffer_now(dma_channel_tx, playback_buffer, BUFFER_SIZE);
        }
    } else if (dma_channel_get_irq0_status(dma_channel_rx)) {
        // Clear the interrupt request.
        dma_hw->ints0 = 1u << dma_channel_rx;

        // TODO Check the checksum and/or read the last input data ?
        // Re-trigger immediately
        dma_channel_start(dma_channel_rx);
    }
}

void gb_serial_init() {
    // Run GB Link at 524288Hz (2 097 152 PIO cycles per second --> clkdiv=59.6046447754 @125MHz)
    // Sending 8 bits takes around 15us, leaving ~145us for the GB to read and process the data
    uint cpha1_prog_offs = pio_add_program(pio, &spi_cpha1_program);
    pio_spi_init(pio, sm, cpha1_prog_offs, 8, 59.6046447754, 1, PIN_SERIAL_CLOCK, PIN_SERIAL_MOSI, PIN_SERIAL_MISO);    // 512 Kbps
    //pio_spi_init(pio, sm, cpha1_prog_offs, 8, 119.209289551, 1, PIN_SERIAL_CLOCK, PIN_SERIAL_MOSI, PIN_SERIAL_MISO);    // 256 Kbps
    //pio_spi_init(pio, sm, cpha1_prog_offs, 8, 476.837158203, 1, PIN_SERIAL_CLOCK, PIN_SERIAL_MOSI, PIN_SERIAL_MISO);    // 64 Kbps

    downsample_init(&downsampler_instance, INPUT_SAMPLE_RATE, DECIMATE_FACTOR);
    encode_init(&encoder_instance);

    // TX DMA
    dma_channel_tx = dma_claim_unused_channel(true);
    //printf("claimed tx channel %d\n", dma_channel_tx);
    dma_channel_config dma_config_tx = dma_channel_get_default_config(dma_channel_tx);
    channel_config_set_transfer_data_size(&dma_config_tx, DMA_SIZE_8);
    // Triggered by timer
    dma_timer = dma_claim_unused_timer(true);
    //printf("claimed timer %d\n", dma_timer);
    dma_timer_set_fraction(dma_timer, 3, 60088);  // 125000000*3/60088 = 6240.8467...Hz
    int treq = dma_get_timer_dreq(dma_timer);
    channel_config_set_dreq(&dma_config_tx, treq);
    dma_channel_configure(dma_channel_tx, &dma_config_tx,
        &pio->txf[sm],          // write address
        NULL,                   // read address
        0,                      // element count (each element is of size transfer_data_size)
        false                   // do not start immediately
    );
    // IRQ
    irq_add_shared_handler(DMA_IRQ_0, dma_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    irq_set_enabled(DMA_IRQ_0, true);
    dma_channel_set_irq0_enabled(dma_channel_tx, true);
    
    // RX DMA
    dma_channel_rx = dma_claim_unused_channel(true);
    //printf("claimed rx channel %d\n", dma_channel_rx);
    dma_channel_config dma_config_rx = dma_channel_get_default_config(dma_channel_rx);
    channel_config_set_transfer_data_size(&dma_config_rx, DMA_SIZE_8);
    channel_config_set_read_increment(&dma_config_rx, false);
    // Triggered by pio RX FIFO
    channel_config_set_dreq(&dma_config_rx, pio_get_dreq(pio, sm, false));
    dma_channel_configure(dma_channel_rx, &dma_config_rx,
        &input,                 // write address
        &pio->rxf[sm],          // read address
        BUFFER_SIZE,            // element count (each element is of size transfer_data_size)
        true                    // start immediately
    );
    // IRQ
    dma_channel_set_irq0_enabled(dma_channel_rx, true);
    //dma_channel_start(dma_channel_rx);
}

void gb_serial_streaming_start() {
    printf("[%llu] gb_serial_streaming_start()\n", time_us_64());

    streaming = true;

    // Start first transfer
    printf("[%llu] PLAY buffer: 0x%p of %d bytes (with IRQ enabled)\n", time_us_64(), playback_buffer, BUFFER_SIZE);
    dma_channel_transfer_from_buffer_now(dma_channel_tx, playback_buffer, BUFFER_SIZE);
}

void gb_serial_streaming_stop() {

    streaming = false;

    // Abort
    //printf("Abort TX dma channel %d\n", dma_channel_tx);
    dma_channel_abort(dma_channel_tx);
    //printf("Abort RX dma channel %d\n", dma_channel_rx);
    //dma_channel_abort(dma_channel_rx);

    // Clear PIO FIFOs
    //printf("Clear PIO FIFOs (levels: tx=%d rx=%d)\n", pio_sm_get_tx_fifo_level(pio, sm), pio_sm_get_rx_fifo_level(pio, sm));
    pio_sm_clear_fifos(pio, sm);

    /* FIXME Should deinit dma and pio when we are done ??

    printf("[%llu] Stopping all dma\n", time_us_64());
    // Disable IRQ _before_ aborting (see RP2040-E13)
    //printf("Disable DMA IRQ: %d\n", DMA_IRQ_0);
    irq_set_enabled(DMA_IRQ_0, false);
    dma_channel_set_irq0_enabled(dma_channel_tx, false);
    dma_channel_set_irq0_enabled(dma_channel_rx, false);
    //printf("Remove DMA IRQ handler: %d %p\n", DMA_IRQ_0, dma_handler);
    // FIXME If we are in a IRQ, we need to ask the handler to remove itself on its next execution somehow ??
    // FIXME Or just keep the IRQ handler always, and enable refill/restart only when streaming audio samples ??
    //printf("irq_has_shared_handler(): %d\n", irq_has_shared_handler(DMA_IRQ_0));
    irq_remove_handler(DMA_IRQ_0, dma_handler);
    //printf("Asking ISR to remove itself from DMA IRQ handlers: %d %p\n", DMA_IRQ_0, dma_handler);
    //should_stop = true;
    // Abort
    //printf("Abort TX dma channel %d\n", dma_channel_tx);
    dma_channel_abort(dma_channel_tx);
    //printf("Abort RX dma channel %d\n", dma_channel_rx);
    dma_channel_abort(dma_channel_rx);
    // Unclaim
    //printf("Unclaim TX dma channel: %d\n", dma_channel_tx);
    dma_channel_unclaim(dma_channel_tx);
    //printf("Unclaim TX dma timer: %d\n", dma_timer);
    dma_timer_unclaim(dma_timer);
    //printf("Unclaim RX dma channel: %d\n", dma_channel_rx);
    dma_channel_unclaim(dma_channel_rx);
    // Clear PIO FIFOs
    //printf("Clear PIO FIFOs (levels: tx=%d rx=%d)\n", pio_sm_get_tx_fifo_level(pio, sm), pio_sm_get_rx_fifo_level(pio, sm));
    pio_sm_clear_fifos(pio, sm);
    printf("[%llu] Stopped serial streaming\n", time_us_64());
    */
}

int gb_serial_streaming_needs_samples() {
    return (filling_buffer != playback_buffer) ? EXPECTED_SAMPLES : 0;
}

void gb_serial_streaming_fill_buffer(int16_t* samples) {
    // Downsample and encode the next samples
    int16_t downsampledAudioData[BUFFER_SIZE];
    unsigned char convertedAudioData[BUFFER_SIZE];
    uint32_t samples_before = downsampler_instance.samples;
    
    // Downsample
    //printf("[%d] DOWNSAMPLE remaining=%d\n", time_us_32(), remainingData);
    downsample(&downsampler_instance, samples, downsampledAudioData, EXPECTED_SAMPLES);
    // Convert int16_t to unsigned char
    for(int i = 0; i < downsampler_instance.samples-samples_before; i++) {
        convertedAudioData[i] = (downsampledAudioData[i] + 32768) >> 8;
    }
    // Encode
    encode(&encoder_instance, convertedAudioData, filling_buffer, downsampler_instance.samples-samples_before);
    //printf("Fill buffer: 0x%p\n", filling_buffer);

    // Swap buffers
    swap_buffers(&filling_buffer);
}

/*void gb_serial_fill_buffer_raw(uint8_t* data, bool swap) {
    //printf("Fill buffer (raw): 0x%p from data 0x%p\n", filling_buffer, data);
    memcpy(filling_buffer, data, BUFFER_SIZE);
    // Swap buffers
    if (swap) {
        swap_buffers(&filling_buffer);
    }
}*/

void gb_serial_streaming_clear_buffers() {
    memset(playback_buffer_0, 0, BUFFER_SIZE);
    memset(playback_buffer_1, 0, BUFFER_SIZE);
    memset(playback_buffer_2, 0, BUFFER_SIZE);

    // TODO Reset buffer positions ?
    /*
    filling_buffer = playback_buffer_0;
    playback_buffer = playback_buffer_2;
    */

    // TODO Debug
    /*printf("\n\n\nPLAYED DATA: %d bytes\n\n\n", played_counter);
    for (int i=0; i<played_counter; i+=BUFFER_SIZE) {
        for (int j=0; j<BUFFER_SIZE; j++) {
            printf("%02x", played[i+j]);
        }
        printf("\n");
    }*/
}

void gb_serial_immediate_transfer(uint8_t* data, int length) {
    // Start transfer
    printf("[%llu] start transfer of %d bytes\n", time_us_64(), length);
    dma_channel_transfer_from_buffer_now(dma_channel_tx, data, length);

}

bool gb_serial_transfer_done() {
    return !dma_channel_is_busy(dma_channel_tx);
}
