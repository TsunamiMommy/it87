// SPDX-License-Identifier: GPL-2.0-or-later
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/io.h>
#include <linux/module.h>
#include <linux/types.h>

#include "smfi.h"

/* The Super I/O configuration ports are already reserved by the caller. */
static void gigabyte_smfi_select(int port, u8 ldn)
{
	outb(0x07, port);
	outb(ldn, port + 1);
}

static u8 gigabyte_smfi_read(int port, u8 reg)
{
	outb(reg, port);
	return inb(port + 1);
}

void gigabyte_smfi_probe(int sioaddr, bool extended, bool can_ecio,
			 bool amd, phys_addr_t *base,
			 bool *mapped, bool *ecio)
{
	u8 enable, low, high, extra;
	u32 address;

	gigabyte_smfi_select(sioaddr, GIGABYTE_SIO_H2RAM);
	enable = gigabyte_smfi_read(sioaddr, IT87_SMFI_ENABLE);
	if (enable) {
		low = gigabyte_smfi_read(sioaddr, IT87_SMFI_BASE_LOW);
		high = gigabyte_smfi_read(sioaddr, IT87_SMFI_BASE_HI);
		if (extended) {
			extra = gigabyte_smfi_read(sioaddr, IT87_SMFI_BASE_EX);
			address = 0xfc000000;
			address |= (u32)high << 16;
			address |= (u32)(low & 0xf0) << 12;
			address |= (u32)(extra & 0x0f) << 24;
		} else {
			address = 0xff000000;
			address |= (u32)high << 16;
			address |= (u32)(low & 0xf0) << 12;
		}
		*base = address;
		*mapped = true;
	} else if (amd && can_ecio) {
		pr_info("AMD platform with ECIO H2RAM detected, enabling ECIO backend\n");
		*ecio = true;
	}
	/* Return to the ordinary hardware-monitoring logical device. */
	gigabyte_smfi_select(sioaddr, 0x04);
}
