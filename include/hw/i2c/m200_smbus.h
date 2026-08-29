/* Toshiba Portégé M200 board SMBus devices. */
#ifndef HW_I2C_M200_SMBUS_H
#define HW_I2C_M200_SMBUS_H

#include "hw/i2c/i2c.h"

#define TYPE_M200_ADM1032 "m200-adm1032"
#define TYPE_M200_ICS950812 "m200-ics950812"

void m200_smbus_create_board_devices(I2CBus *bus);

#endif
