#include <string.h>
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
int dma_channel_rx;

PIO pio = pio0;
uint sm = 0;

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

        // Swap buffers
        swap_buffers(&playback_buffer);

        // Give the channel a new buffer to read from, and re-trigger it
        dma_channel_transfer_from_buffer_now(dma_channel_tx, playback_buffer, BUFFER_SIZE);
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
    // Sending 8 bits takes around 15us, leaving ~210us for the GB to read and process the data
    uint cpha1_prog_offs = pio_add_program(pio, &spi_cpha1_program);
    pio_spi_init(pio, sm, cpha1_prog_offs, 8, 59.6046447754, 1, PIN_SERIAL_CLOCK, PIN_SERIAL_MOSI, PIN_SERIAL_MISO);

    downsample_init(&downsampler_instance, INPUT_SAMPLE_RATE, DECIMATE_FACTOR);
    encode_init(&encoder_instance);
}

void gb_serial_start() {
    // TX DMA
    dma_channel_tx = dma_claim_unused_channel(true);
    dma_channel_config dma_config_tx = dma_channel_get_default_config(dma_channel_tx);
    channel_config_set_transfer_data_size(&dma_config_tx, DMA_SIZE_8);
    // Triggered by timer
    int dma_timer = dma_claim_unused_timer(true);
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
    dma_channel_start(dma_channel_rx);

    // Start first transfer
    dma_channel_transfer_from_buffer_now(dma_channel_tx, playback_buffer, BUFFER_SIZE);
}

int gb_serial_needs_samples() {
    return (filling_buffer != playback_buffer) ? EXPECTED_SAMPLES : 0;
}

void gb_serial_fill_buffer(int16_t* samples) {
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

    // Swap buffers
    swap_buffers(&filling_buffer);
}

void gb_serial_clear_buffers() {
    memset(playback_buffer_0, 0, BUFFER_SIZE);
    memset(playback_buffer_1, 0, BUFFER_SIZE);
    memset(playback_buffer_2, 0, BUFFER_SIZE);
}
