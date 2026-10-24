/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef IT87_GIGABYTE_ECIO_H
#define IT87_GIGABYTE_ECIO_H

#include <linux/types.h>

#define EXT_ECIO_EXTENT 5
#define ECIO_DATA       0x3f0
#define ECIO_CMD_STAT   0x3f4

int gigabyte_ecio_read(u16 reg);
void gigabyte_ecio_write(u16 reg, u8 value);

#endif
