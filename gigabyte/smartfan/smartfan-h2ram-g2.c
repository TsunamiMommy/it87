// SPDX-License-Identifier: GPL-2.0-or-later
#include "smartfan.h"

static const u16 vector_base[] = { 0x950, 0x960, 0x970 };

u16 gigabyte_smartfan_g2_base(int fan)
{
	return vector_base[fan];
}

void gigabyte_smartfan_g2_save(const struct gigabyte_smartfan_io *io,
			      u16 base, u8 primary[5], u8 extra[9])
{
	int i;

	for (i = 0; i < 4; i++)
		primary[i] = io->read(io->data, base + 2 + i);
	primary[4] = 0;
	for (i = 0; i < 3; i++)
		extra[i] = io->read(io->data, base + 6 + i * 2);
}

void gigabyte_smartfan_g2_manual(const struct gigabyte_smartfan_io *io,
				u16 base, u8 pwm)
{
	int i;

	for (i = 0; i < 3; i++)
		io->write(io->data, base + 6 + i * 2, 0x7f);
	io->write(io->data, base + 2, 0x00);
	io->write(io->data, base + 3, 0x7f);
	io->write(io->data, base + 4, pwm);
	io->write(io->data, base + 5, 0x00);
}

void gigabyte_smartfan_g2_restore(const struct gigabyte_smartfan_io *io,
				 u16 base, const u8 primary[5],
				 const u8 extra[9])
{
	int i;

	for (i = 0; i < 3; i++)
		io->write(io->data, base + 6 + i * 2, extra[i]);
	io->write(io->data, base + 5, primary[3]);
	io->write(io->data, base + 4, primary[2]);
	io->write(io->data, base + 3, primary[1]);
	io->write(io->data, base + 2, primary[0]);
}
