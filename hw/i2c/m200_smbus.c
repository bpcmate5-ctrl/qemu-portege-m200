/*
 * Toshiba Portégé M200 board SMBus devices
 *
 * The M200 board documentation identifies an ADM1032 at address 0x4c and
 * an ICS950812 clock generator at address 0x69.  This file models their
 * documented register protocols; transaction timing belongs to the ICH4-M
 * host controller, not to these slaves.
 */

#include "qemu/osdep.h"
#include "hw/i2c/i2c.h"
#include "hw/i2c/smbus_slave.h"
#include "hw/i2c/m200_smbus.h"
#include "migration/vmstate.h"
#include "qemu/module.h"

typedef struct M200SMBusRegisterDevice {
    SMBusDevice parent_obj;
    uint8_t regs[256];
    uint8_t pointer;
} M200SMBusRegisterDevice;

typedef struct M200SMBusRegisterDeviceClass {
    SMBusDeviceClass parent_class;
    void (*reset_registers)(M200SMBusRegisterDevice *s);
    bool (*register_writable)(uint8_t reg);
} M200SMBusRegisterDeviceClass;

#define TYPE_M200_SMBUS_REGISTER_DEVICE "m200-smbus-register-device"
OBJECT_DECLARE_TYPE(M200SMBusRegisterDevice, M200SMBusRegisterDeviceClass,
                    M200_SMBUS_REGISTER_DEVICE)

static uint8_t m200_smbus_reg_receive(SMBusDevice *dev)
{
    M200SMBusRegisterDevice *s = M200_SMBUS_REGISTER_DEVICE(dev);

    return s->regs[s->pointer++];
}

static int m200_smbus_reg_write(SMBusDevice *dev, uint8_t *buf, uint8_t len)
{
    M200SMBusRegisterDevice *s = M200_SMBUS_REGISTER_DEVICE(dev);
    M200SMBusRegisterDeviceClass *sc = M200_SMBUS_REGISTER_DEVICE_GET_CLASS(s);
    unsigned i;

    if (!len) {
        return 0;
    }

    s->pointer = buf[0];
    for (i = 1; i < len; i++, s->pointer++) {
        if (!sc->register_writable || sc->register_writable(s->pointer)) {
            s->regs[s->pointer] = buf[i];
        }
    }
    return 0;
}

static void m200_smbus_reg_reset(DeviceState *dev)
{
    M200SMBusRegisterDevice *s = M200_SMBUS_REGISTER_DEVICE(dev);
    M200SMBusRegisterDeviceClass *sc = M200_SMBUS_REGISTER_DEVICE_GET_CLASS(s);

    memset(s->regs, 0, sizeof(s->regs));
    s->pointer = 0;
    if (sc->reset_registers) {
        sc->reset_registers(s);
    }
}

static const VMStateDescription vmstate_m200_smbus_reg = {
    .name = TYPE_M200_SMBUS_REGISTER_DEVICE,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_SMBUS_DEVICE(parent_obj, M200SMBusRegisterDevice),
        VMSTATE_UINT8_ARRAY(regs, M200SMBusRegisterDevice, 256),
        VMSTATE_UINT8(pointer, M200SMBusRegisterDevice),
        VMSTATE_END_OF_LIST()
    },
};

static void m200_smbus_reg_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SMBusDeviceClass *sc = SMBUS_DEVICE_CLASS(klass);

    device_class_set_legacy_reset(dc, m200_smbus_reg_reset);
    dc->vmsd = &vmstate_m200_smbus_reg;
    sc->write_data = m200_smbus_reg_write;
    sc->receive_byte = m200_smbus_reg_receive;
}

static const TypeInfo m200_smbus_reg_info = {
    .name = TYPE_M200_SMBUS_REGISTER_DEVICE,
    .parent = TYPE_SMBUS_DEVICE,
    .instance_size = sizeof(M200SMBusRegisterDevice),
    .class_size = sizeof(M200SMBusRegisterDeviceClass),
    .class_init = m200_smbus_reg_class_init,
    .abstract = true,
};

static bool adm1032_register_writable(uint8_t reg)
{
    switch (reg) {
    case 0x09 ... 0x0f:
    case 0x11 ... 0x14:
    case 0x19 ... 0x22:
        return true;
    default:
        return false;
    }
}

static uint8_t adm1032_read_alias(uint8_t reg)
{
    switch (reg) {
    case 0x09: return 0x03;
    case 0x0a: return 0x04;
    case 0x0b: return 0x05;
    case 0x0c: return 0x06;
    case 0x0d: return 0x07;
    case 0x0e: return 0x08;
    default:   return reg;
    }
}

static int adm1032_write_data(SMBusDevice *dev, uint8_t *buf, uint8_t len)
{
    M200SMBusRegisterDevice *s = M200_SMBUS_REGISTER_DEVICE(dev);
    uint8_t reg;
    unsigned i;

    if (!len) {
        return 0;
    }

    reg = buf[0];
    s->pointer = reg;
    for (i = 1; i < len; i++, reg++) {
        if (reg != 0x0f && adm1032_register_writable(reg)) {
            s->regs[reg] = buf[i];
            s->regs[adm1032_read_alias(reg)] = buf[i];
        }
    }
    return 0;
}

static void adm1032_reset_registers(M200SMBusRegisterDevice *s)
{
    /* ADM1032 data-sheet Table 12 power-on defaults. */
    s->regs[0x04] = s->regs[0x0a] = 0x08;
    s->regs[0x05] = s->regs[0x0b] = 0x55;
    s->regs[0x07] = s->regs[0x0d] = 0x55;
    s->regs[0x19] = 0x55;
    s->regs[0x20] = 0x55;
    s->regs[0x21] = 0x0a;
    s->regs[0x22] = 0x01;
    s->regs[0xfe] = 0x41; /* Analog Devices manufacturer ID. */
}

static void adm1032_class_init(ObjectClass *klass, const void *data)
{
    M200SMBusRegisterDeviceClass *sc =
        M200_SMBUS_REGISTER_DEVICE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);
    SMBusDeviceClass *smbc = SMBUS_DEVICE_CLASS(klass);

    sc->reset_registers = adm1032_reset_registers;
    sc->register_writable = adm1032_register_writable;
    smbc->write_data = adm1032_write_data;
    dc->desc = "ADM1032 temperature monitor (M200)";
}

static const TypeInfo adm1032_info = {
    .name = TYPE_M200_ADM1032,
    .parent = TYPE_M200_SMBUS_REGISTER_DEVICE,
    .class_init = adm1032_class_init,
};

static bool ics950812_register_writable(uint8_t reg)
{
    /* Bytes 0..7 and 9..14 are the documented programming bytes. */
    return reg <= 7 || (reg >= 9 && reg <= 14);
}

static int ics950812_write_data(SMBusDevice *dev, uint8_t *buf, uint8_t len)
{
    M200SMBusRegisterDevice *s = M200_SMBUS_REGISTER_DEVICE(dev);
    unsigned count, i;

    /*
     * CK408 block programming is: command 00h, byte count, Byte0..ByteN.
     * The count is framing and is not itself clock-generator Byte0.
     */
    if (len >= 2 && buf[0] == 0) {
        count = MIN((unsigned)buf[1], (unsigned)len - 2);
        for (i = 0; i < count && i < 15; i++) {
            if (ics950812_register_writable(i)) {
                s->regs[i] = buf[i + 2];
            }
        }
        s->pointer = 0;
        return 0;
    }

    return m200_smbus_reg_write(dev, buf, len);
}

static void ics950812_class_init(ObjectClass *klass, const void *data)
{
    M200SMBusRegisterDeviceClass *sc =
        M200_SMBUS_REGISTER_DEVICE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);
    SMBusDeviceClass *smbc = SMBUS_DEVICE_CLASS(klass);

    sc->register_writable = ics950812_register_writable;
    smbc->write_data = ics950812_write_data;
    dc->desc = "ICS950812 CK408 clock generator (M200)";
}

static const TypeInfo ics950812_info = {
    .name = TYPE_M200_ICS950812,
    .parent = TYPE_M200_SMBUS_REGISTER_DEVICE,
    .class_init = ics950812_class_init,
};

void m200_smbus_create_board_devices(I2CBus *bus)
{
    i2c_slave_create_simple(bus, TYPE_M200_ADM1032, 0x4c);
    i2c_slave_create_simple(bus, TYPE_M200_ICS950812, 0x69);
}

static void m200_smbus_register_types(void)
{
    type_register_static(&m200_smbus_reg_info);
    type_register_static(&adm1032_info);
    type_register_static(&ics950812_info);
}

type_init(m200_smbus_register_types)
