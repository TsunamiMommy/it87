// SPDX-License-Identifier: GPL-2.0-or-later
#include <linux/bitops.h>

#include "smartfan.h"

int gigabyte_smartfan_channel_count(enum it87_h2ram_smartfan_gen gen,
				   u8 it57xx_fans)
{
	if (gen == IT87_H2RAM_SF_G3)
		return IT87_H2RAM_BASE_FANS + it57xx_fans;
	if (gen == IT87_H2RAM_SF_G2)
		return IT87_H2RAM_BASE_FANS;
	return 0;
}

u16 gigabyte_smartfan_vector_base(enum it87_h2ram_smartfan_gen gen,
				 u8 it57xx_fans, int fan)
{
	if (fan < 0 || fan >= gigabyte_smartfan_channel_count(gen, it57xx_fans))
		return 0;

	if (fan >= IT87_H2RAM_BASE_FANS)
		return gigabyte_smartfan_it57xx_base(fan - IT87_H2RAM_BASE_FANS);
	if (gen == IT87_H2RAM_SF_G2)
		return gigabyte_smartfan_g2_base(fan);
	return gigabyte_smartfan_g3_base(fan);
}

bool gigabyte_smartfan_tach_regs(enum it87_h2ram_smartfan_gen gen,
				 u8 it57xx_version, u8 it57xx_fans,
				 int fan, u16 *lsb, u16 *msb)
{
	if (gen != IT87_H2RAM_SF_G3 || fan < 0 ||
	    fan >= gigabyte_smartfan_channel_count(gen, it57xx_fans))
		return false;

	if (fan < IT87_H2RAM_BASE_FANS)
		return gigabyte_smartfan_g3_tach(fan, lsb, msb);
	return gigabyte_smartfan_it57xx_tach(it57xx_version,
						     it57xx_fans,
						     fan - IT87_H2RAM_BASE_FANS,
						     lsb, msb);
}

void gigabyte_smartfan_save_vector(enum it87_h2ram_smartfan_gen gen,
				  const struct gigabyte_smartfan_io *io,
				  u16 base, u8 primary[5], u8 extra[9])
{
	if (gen == IT87_H2RAM_SF_G2)
		gigabyte_smartfan_g2_save(io, base, primary, extra);
	else
		gigabyte_smartfan_g3_save(io, base, primary, extra);
}

void gigabyte_smartfan_set_manual(enum it87_h2ram_smartfan_gen gen,
				 const struct gigabyte_smartfan_io *io,
				 u16 base, u8 pwm)
{
	if (gen == IT87_H2RAM_SF_G2)
		gigabyte_smartfan_g2_manual(io, base, pwm);
	else
		gigabyte_smartfan_g3_manual(io, base, pwm);
}

void gigabyte_smartfan_restore_vector(enum it87_h2ram_smartfan_gen gen,
				     const struct gigabyte_smartfan_io *io,
				     u16 base, const u8 primary[5],
				     const u8 extra[9])
{
	if (gen == IT87_H2RAM_SF_G2)
		gigabyte_smartfan_g2_restore(io, base, primary, extra);
	else
		gigabyte_smartfan_g3_restore(io, base, primary, extra);
}

/* Firmware generations use distinct register layouts. */
enum it87_h2ram_smartfan_gen
gigabyte_smartfan_detect_g23(bool allow_g3, bool allow_g2,
			     u8 major, u8 minor)
{
	if (allow_g3 && major == 0x01 && minor == 0x00)
		return IT87_H2RAM_SF_G3;
	if (allow_g2 && major == 0x00 &&
	    (minor == 0x01 || minor == 0x03 || minor == 0x04))
		return IT87_H2RAM_SF_G2;
	return IT87_H2RAM_SF_NONE;
}

bool gigabyte_smartfan_detect_g1(bool allow_g1, u8 major, u8 minor)
{
	return allow_g1 && major && minor;
}

u8 gigabyte_smartfan_override_mask(enum it87_h2ram_smartfan_gen gen,
				   u8 it57xx_fans, u8 mask)
{
	int channels;

	if (gen == IT87_H2RAM_SF_G1)
		return mask & GENMASK(IT87_H2RAM_BASE_FANS - 1, 0);

	channels = gigabyte_smartfan_channel_count(gen, it57xx_fans);
	if (!channels)
		return 0;

	return mask & (BIT(channels) - 1);
}

void gigabyte_smartfan_set_enable(const struct gigabyte_smartfan_io *io,
				  bool enable)
{
	int cur = io->read(io->data, GIGABYTE_SMARTFAN_ENABLE_REG);
	u8 val;

	if (cur < 0)
		return;

	val = (u8)cur;
	if (enable)
		val |= BIT(0);
	else
		val &= ~BIT(0);

	if (val == (u8)cur)
		return;

	io->write(io->data, GIGABYTE_SMARTFAN_ENABLE_REG, val);
}

void gigabyte_smartfan_claim(const struct gigabyte_smartfan_io *io,
			     enum it87_h2ram_smartfan_gen gen,
			     u8 *original, bool *saved)
{
	if (gen == IT87_H2RAM_SF_NONE)
		return;

	if (!*saved) {
		*original = io->read(io->data, GIGABYTE_SMARTFAN_ENABLE_REG);
		*saved = true;
	}

	gigabyte_smartfan_set_enable(io, gen != IT87_H2RAM_SF_G1);
}

void gigabyte_smartfan_release(const struct gigabyte_smartfan_io *io,
			       u8 original, bool *saved, bool release)
{
	if (!*saved)
		return;

	io->write(io->data, GIGABYTE_SMARTFAN_ENABLE_REG, original);
	if (release)
		*saved = false;
}
