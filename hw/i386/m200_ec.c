/*
 * Toshiba Portege M200 M306K5 host-interface mailbox
 *
 * Model the DBBIN/DBBOUT ownership rules of the M306K5 LPC host
 * interface.  Toshiba command semantics deliberately belong to a separate
 * backend; this file contains no command-response policy.
 *
 * Copyright (c) 2026
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "hw/i386/m200_ec.h"
#include "qapi/error.h"
#include "hw/core/qdev-properties.h"
#include "qemu/main-loop.h"
#include "migration/vmstate.h"
#include "system/memory.h"

#define M200_EC_STS_OBF 0x01
#define M200_EC_STS_IBF 0x02
#define M200_EC_STS_XA2 0x08
#define M200_EC_STS_USER_MASK 0xf4

struct M200ECMailbox {
    SysBusDevice parent_obj;

    MemoryRegion data_io;
    MemoryRegion command_io;
    QEMUBH *backend_bh;

    uint8_t dbbin;
    uint8_t dbbout;
    bool xa2;
    uint8_t status;
    uint8_t user_status;
    uint8_t reset_user_status;

    const M200ECBackendOps *backend_ops;
    void *backend_opaque;
    bool trace;
};

static void m200_ec_trace(M200ECMailbox *s, const char *event,
                          uint8_t value)
{
    if (s->trace) {
        fprintf(stderr, "[M200 EC MAILBOX] %-18s %02x IBF=%u OBF=%u\n",
                event, value, !!(s->status & M200_EC_STS_IBF),
                !!(s->status & M200_EC_STS_OBF));
    }
}

bool m200_ec_mailbox_input_pending(const M200ECMailbox *s)
{
    return s->status & M200_EC_STS_IBF;
}

uint8_t m200_ec_mailbox_consume_input(M200ECMailbox *s, bool *is_command)
{
    uint8_t value = s->dbbin;

    if (!(s->status & M200_EC_STS_IBF)) {
        if (is_command) {
            *is_command = false;
        }
        return 0;
    }

    if (is_command) {
        *is_command = s->xa2;
    }
    s->status &= ~M200_EC_STS_IBF;
    m200_ec_trace(s, s->xa2 ? "EC consumed CMD" :
                              "EC consumed DATA", value);
    return value;
}

bool m200_ec_mailbox_output_byte(M200ECMailbox *s, uint8_t value)
{
    if (s->status & M200_EC_STS_OBF) {
        return false;
    }

    s->dbbout = value;
    s->status |= M200_EC_STS_OBF;
    m200_ec_trace(s, "DBBOUT <-", value);
    return true;
}

void m200_ec_mailbox_set_user_status(M200ECMailbox *s, uint8_t value)
{
    s->user_status = value & M200_EC_STS_USER_MASK;
}

static void m200_ec_default_input(void *opaque, M200ECMailbox *s)
{
    bool is_command;
    uint8_t value = m200_ec_mailbox_consume_input(s, &is_command);

    if (s->trace) {
        fprintf(stderr, "[M200 EC BACKEND] unsupported %s byte %02x consumed\n",
                is_command ? "command" : "data", value);
    }
}

static const M200ECBackendOps m200_ec_default_backend_ops = {
    .input_available = m200_ec_default_input,
};

static void m200_ec_backend_bh(void *opaque)
{
    M200ECMailbox *s = opaque;

    if (!(s->status & M200_EC_STS_IBF)) {
        return;
    }

    if (s->backend_ops && s->backend_ops->input_available) {
        s->backend_ops->input_available(s->backend_opaque, s);
    }
}

static void m200_ec_host_write(M200ECMailbox *s, uint8_t value,
                               bool is_command)
{
    /*
     * DBBIN is a single hardware latch.  The M306K5 documentation does not
     * interlock a host write while IBF is set: another LPC write replaces
     * DBBIN and IBF remains asserted.  Well-behaved firmware waits for IBF
     * to clear and therefore never relies on this overrun behavior.
     */
    if (s->status & M200_EC_STS_IBF) {
        m200_ec_trace(s, "HOST overwrote DBBIN", value);
    }

    s->dbbin = value;
    /* Port 62h/66h are distinguished by address bit A2, latched as XA2. */
    s->xa2 = is_command;
    s->status |= M200_EC_STS_IBF;
    m200_ec_trace(s, is_command ? "HOST CMD DBBIN <-" :
                                     "HOST DATA DBBIN <-", value);
    if (s->backend_ops && s->backend_ops->input_available) {
        s->backend_ops->input_available(s->backend_opaque, s);
    } else {
        qemu_bh_schedule(s->backend_bh);
    }
}

static uint64_t m200_ec_data_read(void *opaque, hwaddr addr, unsigned size)
{
    M200ECMailbox *s = opaque;
    uint8_t value = s->dbbout;

    if (s->status & M200_EC_STS_OBF) {
        s->status &= ~M200_EC_STS_OBF;
        m200_ec_trace(s, "HOST read DBBOUT", value);
        if (s->backend_ops && s->backend_ops->output_consumed) {
            s->backend_ops->output_consumed(s->backend_opaque, s);
        }
        return value;
    }

    m200_ec_trace(s, "HOST read empty", 0);
    return 0;
}

static void m200_ec_data_write(void *opaque, hwaddr addr, uint64_t value,
                               unsigned size)
{
    m200_ec_host_write(opaque, value, false);
}

static uint64_t m200_ec_status_read(void *opaque, hwaddr addr, unsigned size)
{
    M200ECMailbox *s = opaque;

    return (s->user_status & M200_EC_STS_USER_MASK) |
           (s->xa2 ? M200_EC_STS_XA2 : 0) |
           (s->status & (M200_EC_STS_IBF | M200_EC_STS_OBF));
}

static void m200_ec_command_write(void *opaque, hwaddr addr, uint64_t value,
                                  unsigned size)
{
    m200_ec_host_write(opaque, value, true);
}

static const MemoryRegionOps m200_ec_data_ops = {
    .read = m200_ec_data_read,
    .write = m200_ec_data_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 1,
    .valid.max_access_size = 1,
};

static const MemoryRegionOps m200_ec_command_ops = {
    .read = m200_ec_status_read,
    .write = m200_ec_command_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 1,
    .valid.max_access_size = 1,
};

static void m200_ec_mailbox_reset(DeviceState *dev)
{
    M200ECMailbox *s = M200_EC_MAILBOX(dev);

    qemu_bh_cancel(s->backend_bh);
    s->dbbin = 0;
    s->dbbout = 0;
    s->xa2 = false;
    s->status = 0;

    /* Silicon DBBSTS user flags reset to zero unless board policy says so. */
    s->user_status = s->reset_user_status & M200_EC_STS_USER_MASK;
}

static int m200_ec_mailbox_post_load(void *opaque, int version_id)
{
    M200ECMailbox *s = opaque;

    if (s->status & M200_EC_STS_IBF) {
        qemu_bh_schedule(s->backend_bh);
    }
    return 0;
}

static const VMStateDescription vmstate_m200_ec_mailbox = {
    .name = TYPE_M200_EC_MAILBOX,
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = m200_ec_mailbox_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT8(dbbin, M200ECMailbox),
        VMSTATE_UINT8(dbbout, M200ECMailbox),
        VMSTATE_BOOL(xa2, M200ECMailbox),
        VMSTATE_UINT8(status, M200ECMailbox),
        VMSTATE_UINT8(user_status, M200ECMailbox),
        VMSTATE_END_OF_LIST()
    },
};

static void m200_ec_mailbox_init(Object *obj)
{
    M200ECMailbox *s = M200_EC_MAILBOX(obj);

    s->trace = getenv("M200_EC_MAILBOX_TRACE") != NULL;
    s->backend_ops = &m200_ec_default_backend_ops;
    memory_region_init_io(&s->data_io, obj, &m200_ec_data_ops, s,
                          "m200-ec-data", 1);
    memory_region_init_io(&s->command_io, obj, &m200_ec_command_ops, s,
                          "m200-ec-command-status", 1);
}

static void m200_ec_mailbox_realize(DeviceState *dev, Error **errp)
{
    M200ECMailbox *s = M200_EC_MAILBOX(dev);

    s->backend_bh = qemu_bh_new(m200_ec_backend_bh, s);
}

static void m200_ec_mailbox_unrealize(DeviceState *dev)
{
    M200ECMailbox *s = M200_EC_MAILBOX(dev);

    qemu_bh_delete(s->backend_bh);
    s->backend_bh = NULL;
}

static void m200_ec_mailbox_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    static const Property properties[] = {
        /* Board/backend initial state; the M306K5 silicon default is zero. */
        DEFINE_PROP_UINT8("user-status-reset", M200ECMailbox,
                          reset_user_status, 0),
    };

    dc->realize = m200_ec_mailbox_realize;
    dc->unrealize = m200_ec_mailbox_unrealize;
    device_class_set_legacy_reset(dc, m200_ec_mailbox_reset);
    dc->vmsd = &vmstate_m200_ec_mailbox;
    device_class_set_props(dc, properties);
}

static const TypeInfo m200_ec_mailbox_info = {
    .name = TYPE_M200_EC_MAILBOX,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(M200ECMailbox),
    .instance_init = m200_ec_mailbox_init,
    .class_init = m200_ec_mailbox_class_init,
};

static void m200_ec_mailbox_register_types(void)
{
    type_register_static(&m200_ec_mailbox_info);
}

type_init(m200_ec_mailbox_register_types)

M200ECMailbox *m200_ec_mailbox_new(Object *owner, MemoryRegion *system_io,
                                   uint8_t reset_user_status)
{
    DeviceState *dev = qdev_new(TYPE_M200_EC_MAILBOX);
    M200ECMailbox *s = M200_EC_MAILBOX(dev);

    object_property_add_child(owner, "m200-ec-mailbox", OBJECT(dev));
    qdev_prop_set_uint8(dev, "user-status-reset", reset_user_status);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    memory_region_add_subregion(system_io, 0x62, &s->data_io);
    memory_region_add_subregion(system_io, 0x66, &s->command_io);
    return s;
}

void m200_ec_mailbox_set_backend(M200ECMailbox *s,
                                 const M200ECBackendOps *ops,
                                 void *opaque)
{
    s->backend_ops = ops ? ops : &m200_ec_default_backend_ops;
    s->backend_opaque = opaque;
}
