/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef IT87_GIGABYTE_IO_H
#define IT87_GIGABYTE_IO_H

#include <linux/io.h>
#include <linux/types.h>

#define GIGABYTE_H2RAM_LIMIT	0xfff

/* Gigabyte register transport selected during device initialization. */
enum gigabyte_io_mode {
	GIGABYTE_IO_BRIDGE,
	GIGABYTE_IO_H2RAM,
	GIGABYTE_IO_ECIO,
};

struct gigabyte_io {
	void __iomem *mmio;
	void *private;
	int (*legacy_read)(void *private, u16 reg);
	void (*legacy_write)(void *private, u16 reg, u8 value);
	enum gigabyte_io_mode mode;
	u8 slot;
	bool direct;
	bool bridge_enabled;
	bool it8688;
};

int gigabyte_io_read(struct gigabyte_io *io, u16 reg);
void gigabyte_io_write(struct gigabyte_io *io, u16 reg, u8 value);

#endif
