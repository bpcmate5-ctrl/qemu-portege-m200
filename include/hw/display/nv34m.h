/*
 * NVIDIA GeForce FX Go5200 (NV34M) Graphics Controller
 *
 * Copyright (c) 2024
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 */

#ifndef HW_NV34M_H
#define HW_NV34M_H

#include "exec/hwaddr.h"

#define TYPE_NV34M_VGA "nv34m-vga"

void nv34m_vga_write_text(int row, int col, uint8_t ch, uint8_t attr);
void nv34m_vga_clear_screen(uint8_t attr);
void nv34m_vga_set_mode(int width, int height, int bpp);
void nv34m_vga_sync_ram_buffer(const uint8_t *ram);
void nv34m_set_system_ram(uint8_t *ram);
void nv34m_log_vga_mem_write(hwaddr addr, unsigned size);
void nv34m_log_vga_ioport_write(uint32_t addr, uint32_t val);
void nv34m_log_vga_ioport_read(uint32_t addr, uint32_t val);
void nv34m_dump_vga_state_snapshot(const char *reason);
uint32_t pc_m200_get_cpu_eip(void);

#endif
