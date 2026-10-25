#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/structs/padsbank0.h"

#include "capture_format.h"
#include "lpc_capture.pio.h"

#define CAPTURE_BYTES (240u * 1024u)
#define CAPTURE_WORDS (CAPTURE_BYTES / sizeof(uint32_t))
#define EXPECTED_LCLK_HZ 33000000u
#define CAPTURE_PIO pio0

static uint32_t capture_words[CAPTURE_WORDS] __attribute__((aligned(4)));
static volatile bool dma_complete;
static int capture_dma_chan;

static void dma_done(void)
{
    dma_hw->ints0 = 1u << capture_dma_chan;
    dma_complete = true;
}

static void pins_input_only(void)
{
    for (uint gpio = 2; gpio <= 9; gpio++) {
        gpio_init(gpio);
        gpio_set_dir(gpio, GPIO_IN);
        gpio_set_pulls(gpio, false, false);
        gpio_set_input_hysteresis_enabled(gpio, true);
    }
}

static void write_all(const void *data, size_t len)
{
    const uint8_t *p = data;
    while (len--) {
        putchar_raw(*p++);
    }
    stdio_flush();
}

int main(void)
{
    /* 133 MHz is the documented RP2040 maximum operating frequency. */
    set_sys_clock_khz(133000, true);
    pins_input_only();
    stdio_usb_init();
    sleep_ms(1500);

    PIO pio = CAPTURE_PIO;
    uint sm = pio_claim_unused_sm(pio, true);
    uint offset = pio_add_program(pio, &lpc_capture_program);
    lpc_capture_program_init(pio, sm, offset);

    /* Bypass synchronizers only on captured pins; bench validation is
     * mandatory before this mode is approved for motherboard use. */
    pio->input_sync_bypass |= 0xffu << 2;

    int dma_chan = dma_claim_unused_channel(true);
    capture_dma_chan = dma_chan;
    dma_channel_config dc = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dc, DMA_SIZE_32);
    channel_config_set_read_increment(&dc, false);
    channel_config_set_write_increment(&dc, true);
    channel_config_set_dreq(&dc, pio_get_dreq(pio, sm, false));

    printf("M200 LPC sniffer ready; send A to arm\n");
    for (;;) {
        int c;
        do {
            c = getchar_timeout_us(100000);
        } while (c != 'A' && c != 'a');

        pins_input_only();
        memset(capture_words, 0, sizeof(capture_words));
        dma_complete = false;
        dma_channel_set_irq0_enabled(dma_chan, true);
        irq_set_exclusive_handler(DMA_IRQ_0, dma_done);
        irq_set_enabled(DMA_IRQ_0, true);

        pio_sm_set_enabled(pio, sm, false);
        pio_sm_clear_fifos(pio, sm);
        pio_sm_restart(pio, sm);
        pio_sm_exec(pio, sm, pio_encode_jmp(offset));

        dma_channel_configure(dma_chan, &dc, capture_words,
                              &pio->rxf[sm], CAPTURE_WORDS, true);
        pio_sm_set_enabled(pio, sm, true);

        /* No stdio/USB work occurs while PIO/DMA owns the capture path. */
        while (!dma_complete) {
            tight_loop_contents();
        }
        pio_sm_set_enabled(pio, sm, false);

        M200LPCCaptureHeader h = { 0 };
        memcpy(h.magic, M200_LPC_MAGIC, sizeof(h.magic));
        h.version = M200_LPC_VERSION;
        h.header_bytes = sizeof(h);
        h.flags = M200_LPC_FLAG_OVERFLOW | M200_LPC_FLAG_TRIGGERED |
                  M200_LPC_FLAG_SYNC_BYPASS | M200_LPC_FLAG_TWO_PHASE;
        h.rp2040_sys_hz = clock_get_hz(clk_sys);
        h.expected_lclk_hz = EXPECTED_LCLK_HZ;
        h.sample_count = CAPTURE_BYTES;
        h.payload_bytes = CAPTURE_BYTES;
        h.input_gpio_base = 2;
        h.lclk_gpio = 7;
        h.lframe_gpio = 6;

        printf("M200LPC-BINARY %u\n", (unsigned)(sizeof(h) + CAPTURE_BYTES));
        stdio_flush();
        write_all(&h, sizeof(h));
        write_all(capture_words, CAPTURE_BYTES);
        printf("\nM200LPC-END\n");
    }
}
