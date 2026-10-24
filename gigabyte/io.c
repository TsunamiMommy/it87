// SPDX-License-Identifier: GPL-2.0-or-later
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mutex.h>

#include "bridge.h"
#include "ecio.h"
#include "io.h"

#define GIGABYTE_H2RAM_START	0x800
#define GIGABYTE_H2RAM_END	0xfff
#define GIGABYTE_MMIO_BANK	0x06

static int gigabyte_bridge_read(struct gigabyte_io *io, u16 reg)
{
	int value = 0;

	if (!io->mmio || !io->bridge_enabled)
		return 0;

	mutex_lock(&gigabyte_bridge_mutex);
	if (gigabyte_bridge_ready() && !gigabyte_bridge_use_slot(io->slot))
		value = readb(io->mmio + reg);
	mutex_unlock(&gigabyte_bridge_mutex);

	return value;
}

static void gigabyte_espi_write(struct gigabyte_io *io, u16 reg, u8 value)
{
	u8 saved, bank;

	if (reg == GIGABYTE_MMIO_BANK) {
		writeb(value, io->mmio + reg);
		return;
	}

	bank = (reg >> 8) & 0x03;
	saved = readb(io->mmio + GIGABYTE_MMIO_BANK);
	if (((saved >> 5) & 0x03) != bank)
		writeb((saved & ~GENMASK(6, 5)) | (bank << 5),
		       io->mmio + GIGABYTE_MMIO_BANK);

	writeb(value, io->mmio + reg);
	if (((saved >> 5) & 0x03) != bank)
		writeb(saved, io->mmio + GIGABYTE_MMIO_BANK);
}

static void gigabyte_bridge_write(struct gigabyte_io *io, u16 reg, u8 value)
{
	if (!io->mmio || !io->bridge_enabled)
		return;

	mutex_lock(&gigabyte_bridge_mutex);
	if (gigabyte_bridge_ready() && !gigabyte_bridge_use_slot(io->slot)) {
		if (io->it8688 && gigabyte_bridge_intel_espi())
			gigabyte_espi_write(io, reg, value);
		else
			writeb(value, io->mmio + reg);
	}
	mutex_unlock(&gigabyte_bridge_mutex);
}

int gigabyte_io_read(struct gigabyte_io *io, u16 reg)
{
	switch (io->mode) {
	case GIGABYTE_IO_BRIDGE:
		return gigabyte_bridge_read(io, reg);
	case GIGABYTE_IO_H2RAM:
		if (reg >= GIGABYTE_H2RAM_START &&
		    reg <= GIGABYTE_H2RAM_END) {
			if (io->direct)
				return readb(io->mmio + reg);
			return gigabyte_bridge_read(io, reg);
		}
		return io->legacy_read(io->private, reg);
	case GIGABYTE_IO_ECIO:
		if (reg >= GIGABYTE_H2RAM_START &&
		    reg <= GIGABYTE_H2RAM_END)
			return gigabyte_ecio_read(reg);
		return io->legacy_read(io->private, reg);
	}

	return 0;
}

void gigabyte_io_write(struct gigabyte_io *io, u16 reg, u8 value)
{
	switch (io->mode) {
	case GIGABYTE_IO_BRIDGE:
		gigabyte_bridge_write(io, reg, value);
		break;
	case GIGABYTE_IO_H2RAM:
		if (reg >= GIGABYTE_H2RAM_START &&
		    reg <= GIGABYTE_H2RAM_END) {
			if (io->direct)
				writeb(value, io->mmio + reg);
			else
				gigabyte_bridge_write(io, reg, value);
		} else {
			io->legacy_write(io->private, reg, value);
		}
		break;
	case GIGABYTE_IO_ECIO:
		if (reg >= GIGABYTE_H2RAM_START &&
		    reg <= GIGABYTE_H2RAM_END)
			gigabyte_ecio_write(reg, value);
		else
			io->legacy_write(io->private, reg, value);
		break;
	}
}
