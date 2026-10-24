// SPDX-License-Identifier: GPL-2.0-or-later
#include <linux/bitops.h>

#include "smartfan.h"

#define IT87_EXTVEC_B2_ENABLE(nr, vec) (0x212 + (nr) * 0x10 + (vec) * 4)
#define IT87_EXTVEC_B3_ENABLE(nr) (0x31c + (nr) * 0x10)

void gigabyte_smartfan_extvec_disable(const struct gigabyte_smartfan_io *io,
				      int fan, bool *saved, u8 snapshot[4])
{
	u16 reg;
	int i;

	if (!*saved) {
		for (i = 0; i < 3; i++)
			snapshot[i] = io->read(io->data,
					       IT87_EXTVEC_B2_ENABLE(fan, i));
		snapshot[3] = io->read(io->data, IT87_EXTVEC_B3_ENABLE(fan));
		*saved = true;
	}

	for (i = 0; i < 3; i++) {
		reg = IT87_EXTVEC_B2_ENABLE(fan, i);
		io->write(io->data, reg, io->read(io->data, reg) & ~BIT(7));
	}

	reg = IT87_EXTVEC_B3_ENABLE(fan);
	io->write(io->data, reg, io->read(io->data, reg) & ~GENMASK(5, 0));
}

void gigabyte_smartfan_extvec_restore(const struct gigabyte_smartfan_io *io,
				      int fan, bool *saved,
				      const u8 snapshot[4], bool release)
{
	int i;

	if (!*saved)
		return;

	for (i = 0; i < 3; i++)
		io->write(io->data, IT87_EXTVEC_B2_ENABLE(fan, i), snapshot[i]);
	io->write(io->data, IT87_EXTVEC_B3_ENABLE(fan), snapshot[3]);
	if (release)
		*saved = false;
}
