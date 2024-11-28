#include "hardware/dma.h"
#include "hardware/pio.h"
#include "spi.pio.h"

#include "gb_serial.h"

int input;

int dma_channel_tx;
int dma_timer;
int dma_channel_rx;

PIO pio = pio0;
uint sm = 0;

gb_serial_source_t* _source;


void __isr __time_critical_func(dma_handler)() {
    if (dma_channel_get_irq0_status(dma_channel_tx)) {
        // Clear the interrupt request.
        dma_hw->ints0 = 1u << dma_channel_tx;
        if (_source != NULL) {
            // Swap buffers
            _source->swap();
            // Give the channel a new buffer to read from, and re-trigger it
            dma_channel_transfer_from_buffer_now(dma_channel_tx, _source->playback_buffer(), _source->buffer_size);
        }
    } else if (dma_channel_get_irq0_status(dma_channel_rx)) {
        // Clear the interrupt request.
        dma_hw->ints0 = 1u << dma_channel_rx;
        // Re-trigger immediately
        dma_channel_start(dma_channel_rx);
    }
}

void gb_serial_init() {
    // Run GB Link at 524288Hz (2 097 152 PIO cycles per second --> clkdiv=59.6046447754 @125MHz)
    // Sending 8 bits takes around 15us, leaving ~145us for the GB to read and process the data
    uint cpha1_prog_offs = pio_add_program(pio, &spi_cpha1_program);
    pio_spi_init(pio, sm, cpha1_prog_offs, 8, 59.6046447754, 1, PIN_SERIAL_CLOCK, PIN_SERIAL_MOSI, PIN_SERIAL_MISO);

    // TX DMA
    dma_channel_tx = dma_claim_unused_channel(true);
    dma_channel_config dma_config_tx = dma_channel_get_default_config(dma_channel_tx);
    channel_config_set_transfer_data_size(&dma_config_tx, DMA_SIZE_8);
    // Triggered by timer
    dma_timer = dma_claim_unused_timer(true);
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
        0xffffffff,             // element count (each element is of size transfer_data_size)
        true                    // start immediately
    );
    // IRQ
    dma_channel_set_irq0_enabled(dma_channel_rx, true);
}

void gb_serial_streaming_start(gb_serial_source_t* source) {
    _source = source;

    // Start first transfer
    dma_channel_transfer_from_buffer_now(dma_channel_tx, _source->playback_buffer(), _source->buffer_size);
}

void gb_serial_streaming_stop() {
    _source = NULL;

    // Abort
    dma_channel_abort(dma_channel_tx);

    // Clear PIO FIFOs
    pio_sm_clear_fifos(pio, sm);
}

void gb_serial_immediate_transfer(uint8_t* data, uint16_t length) {
    // Start transfer
    dma_channel_transfer_from_buffer_now(dma_channel_tx, data, length);
}

bool gb_serial_transfer_done() {
    return !dma_channel_is_busy(dma_channel_tx);
}
