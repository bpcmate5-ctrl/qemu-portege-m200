#ifndef M200_LPC_CAPTURE_FORMAT_H
#define M200_LPC_CAPTURE_FORMAT_H

#include <stdint.h>

#define M200_LPC_MAGIC "M2LPRAW"
#define M200_LPC_VERSION 1
#define M200_LPC_FLAG_OVERFLOW (1u << 0)
#define M200_LPC_FLAG_TRIGGERED (1u << 1)
#define M200_LPC_FLAG_SYNC_BYPASS (1u << 2)
#define M200_LPC_FLAG_TWO_PHASE (1u << 3)

/*
 * Payload is byte-addressable despite DMA using 32-bit transfers.  Each byte:
 *   bits 3:0 LAD[3:0], bit 4 LFRAME#, bit 5 observed LCLK, bits 7:6 reserved.
 * In TWO_PHASE mode each LPC clock produces a low-phase sample immediately
 * before the rising edge and a high-phase sample immediately after it.
 * Four chronological raw samples occupy each little-endian DMA word.
 */
typedef struct __attribute__((packed)) M200LPCCaptureHeader {
    char magic[8];
    uint16_t version;
    uint16_t header_bytes;
    uint32_t flags;
    uint32_t rp2040_sys_hz;
    uint32_t expected_lclk_hz;
    uint32_t sample_count;
    uint32_t payload_bytes;
    uint32_t input_gpio_base;
    uint32_t lclk_gpio;
    uint32_t lframe_gpio;
    uint32_t reserved[5];
} M200LPCCaptureHeader;

_Static_assert(sizeof(M200LPCCaptureHeader) == 64,
               "capture header must be 64 bytes");

#endif
