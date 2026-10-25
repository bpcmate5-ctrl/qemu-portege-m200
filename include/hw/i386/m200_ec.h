/*
 * Toshiba Portege M200 M306K5 host-interface mailbox
 *
 * Copyright (c) 2026
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef HW_I386_M200_EC_H
#define HW_I386_M200_EC_H

#include "hw/core/sysbus.h"

typedef struct MemoryRegion MemoryRegion;

#define TYPE_M200_EC_MAILBOX "m200-ec-mailbox"
OBJECT_DECLARE_SIMPLE_TYPE(M200ECMailbox, M200_EC_MAILBOX)

typedef struct M200ECBackendOps {
    /* Called after the host has filled DBBIN and asserted IBF. */
    void (*input_available)(void *opaque, M200ECMailbox *mailbox);
    /* Called after the host has read DBBOUT and cleared OBF. */
    void (*output_consumed)(void *opaque, M200ECMailbox *mailbox);
} M200ECBackendOps;

M200ECMailbox *m200_ec_mailbox_new(Object *owner, MemoryRegion *system_io,
                                   uint8_t reset_user_status);
void m200_ec_mailbox_set_backend(M200ECMailbox *mailbox,
                                 const M200ECBackendOps *ops,
                                 void *opaque);

bool m200_ec_mailbox_input_pending(const M200ECMailbox *mailbox);
uint8_t m200_ec_mailbox_consume_input(M200ECMailbox *mailbox,
                                      bool *is_command);
bool m200_ec_mailbox_output_byte(M200ECMailbox *mailbox, uint8_t value);
void m200_ec_mailbox_set_user_status(M200ECMailbox *mailbox, uint8_t value);

#endif
