#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "lpc_generator.pio.h"

#define GEN_PIO pio0
#define HALF_SYMBOL_MAX 1024
#ifndef GENERATOR_LCLK_HZ
#define GENERATOR_LCLK_HZ 33000000u
#endif

static uint8_t half_symbols[HALF_SYMBOL_MAX] __attribute__((aligned(4)));
static size_t half_count;

static void half(uint8_t lad, bool frame, bool clk)
{
    if (half_count >= HALF_SYMBOL_MAX) {
        panic("generator vector overflow");
    }
    half_symbols[half_count++] = (lad & 0x0f) | (frame ? 0x10 : 0) |
                                 (clk ? 0x20 : 0);
}

static void cycle(uint8_t lad, bool frame)
{
    half(lad, frame, false);
    half(lad, frame, true);
}

static void io_cycle(bool write, uint16_t addr, uint8_t data)
{
    cycle(0x0, false);                 /* START */
    cycle(write ? 0x2 : 0x0, true);   /* CYCTYPE/DIR */
    cycle((addr >> 12) & 0xf, true);
    cycle((addr >> 8) & 0xf, true);
    cycle((addr >> 4) & 0xf, true);
    cycle(addr & 0xf, true);
    if (write) {
        cycle(data & 0xf, true);
        cycle(data >> 4, true);
        cycle(0xf, true);              /* TAR */
        cycle(0xf, true);
        cycle(0x0, true);              /* SYNC ready */
    } else {
        cycle(0xf, true);              /* TAR */
        cycle(0xf, true);
        cycle(0x0, true);              /* SYNC ready */
        cycle(data & 0xf, true);
        cycle(data >> 4, true);
    }
    cycle(0xf, true);                  /* TAR */
    cycle(0xf, true);
    for (int i = 0; i < 3; i++) {
        cycle(0xf, true);              /* idle separation */
    }
}

int main(void)
{
    set_sys_clock_khz(133000, true);
    stdio_init_all();

    io_cycle(true, 0x0066, 0x9e);
    io_cycle(true, 0x0062, 0x11);
    io_cycle(true, 0x0062, 0x10);
    io_cycle(true, 0x0066, 0x9f);
    io_cycle(false, 0x0066, 0x71);
    io_cycle(false, 0x0062, 0xa5);
    io_cycle(false, 0x0062, 0x5a);

    PIO pio = GEN_PIO;
    uint sm = pio_claim_unused_sm(pio, true);
    uint offset = pio_add_program(pio, &lpc_generator_program);
    float symbol_hz = 2.0f * GENERATOR_LCLK_HZ;
    lpc_generator_program_init(pio, sm, offset,
                               (float)clock_get_hz(clk_sys) / symbol_hz);

    int ch = dma_claim_unused_channel(true);
    dma_channel_config dc = dma_channel_get_default_config(ch);
    channel_config_set_transfer_data_size(&dc, DMA_SIZE_32);
    channel_config_set_read_increment(&dc, true);
    channel_config_set_write_increment(&dc, false);
    channel_config_set_dreq(&dc, pio_get_dreq(pio, sm, true));

    /* Length is always a multiple of four half-symbols for this vector. */
    dma_channel_configure(ch, &dc, &pio->txf[sm], half_symbols,
                          half_count / 4, true);
    pio_sm_set_enabled(pio, sm, true);
    dma_channel_wait_for_finish_blocking(ch);
    pio_sm_set_enabled(pio, sm, false);

    /* Put all generator pins in high-impedance state after the vector. */
    pio_sm_set_consecutive_pindirs(pio, sm, 2, 8, false);
    while (true) {
        tight_loop_contents();
    }
}
