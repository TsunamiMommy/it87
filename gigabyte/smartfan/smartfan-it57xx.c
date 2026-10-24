// SPDX-License-Identifier: GPL-2.0-or-later
#include "smartfan.h"

static const u16 vector_base[] = { 0xc30, 0xc60, 0xc90, 0xcc0 };
static const u16 v1_tach_lsb[] = { 0xc0a, 0xc0c };
static const u16 v1_tach_msb[] = { 0xc0b, 0xc0d };
static const u16 v2_tach_lsb[] = { 0xc10, 0xc12, 0xc14, 0xc16 };
static const u16 v2_tach_msb[] = { 0xc11, 0xc13, 0xc15, 0xc17 };

u16 gigabyte_smartfan_it57xx_base(int fan)
{
	return vector_base[fan];
}

bool gigabyte_smartfan_it57xx_tach(u8 version, u8 count, int fan,
				   u16 *lsb, u16 *msb)
{
	if (fan < 0 || fan >= count)
		return false;

	if (version == 1) {
		if (fan >= 2)
			return false;
		*lsb = v1_tach_lsb[fan];
		*msb = v1_tach_msb[fan];
	} else {
		if (fan >= 4)
			return false;
		*lsb = v2_tach_lsb[fan];
		*msb = v2_tach_msb[fan];
	}
	return true;
}

void gigabyte_smartfan_it57xx_detect(u8 major, u8 minor, bool ecio,
				     u8 *version, u8 *count)
{
	if (minor == 0x00 && major == 0x01) {
		*version = 1;
		*count = 2;
	} else if (minor == 0x00 && major == 0x02) {
		*version = 2;
		*count = ecio ? 2 : 4;
	}
}
