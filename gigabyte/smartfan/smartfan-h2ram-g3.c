// SPDX-License-Identifier: GPL-2.0-or-later
#include "smartfan.h"

static const u16 vector_base[] = { 0x950, 0x980, 0x9b0 };
static const u16 tach_lsb[] = { 0x916, 0x918, 0x91a };
static const u16 tach_msb[] = { 0x917, 0x919, 0x91b };

u16 gigabyte_smartfan_g3_base(int fan)
{
	return vector_base[fan];
}

bool gigabyte_smartfan_g3_tach(int fan, u16 *lsb, u16 *msb)
{
	*lsb = tach_lsb[fan];
	*msb = tach_msb[fan];
	return true;
}

void gigabyte_smartfan_g3_save(const struct gigabyte_smartfan_io *io,
			      u16 base, u8 primary[5], u8 extra[9])
{
	int i;

	for (i = 0; i < 5; i++)
		primary[i] = io->read(io->data, base + 2 + i);
	for (i = 0; i < IT87_H2RAM_MAX_EXTRA_VECTORS; i++)
		extra[i] = io->read(io->data, base + 7 + i * 4);
}

void gigabyte_smartfan_g3_manual(const struct gigabyte_smartfan_io *io,
				u16 base, u8 pwm)
{
	int i;

	for (i = 0; i < IT87_H2RAM_MAX_EXTRA_VECTORS; i++)
		io->write(io->data, base + 7 + i * 4, 0x64);
	io->write(io->data, base + 2, 0x00);
	io->write(io->data, base + 3, 0x7f);
	io->write(io->data, base + 4, pwm);
	io->write(io->data, base + 5, 0x00);
	io->write(io->data, base + 6, 0x00);
}

void gigabyte_smartfan_g3_restore(const struct gigabyte_smartfan_io *io,
				 u16 base, const u8 primary[5],
				 const u8 extra[9])
{
	int i;

	for (i = 0; i < IT87_H2RAM_MAX_EXTRA_VECTORS; i++)
		io->write(io->data, base + 7 + i * 4, extra[i]);
	io->write(io->data, base + 6, primary[4]);
	io->write(io->data, base + 5, primary[3]);
	io->write(io->data, base + 4, primary[2]);
	io->write(io->data, base + 3, primary[1]);
	io->write(io->data, base + 2, primary[0]);
}
