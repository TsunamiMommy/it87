/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef IT87_GIGABYTE_PLATFORM_H
#define IT87_GIGABYTE_PLATFORM_H

#include <linux/types.h>

/* Decoded motherboard identity, independent of register transport. */
struct gbw_siv_info {
	u32 siv;
	u8 platform;
	u8 special;
	u8 fan_count;
	u8 temp_count;
	u8 volt_count;
	bool supported;
};

int gigabyte_platform_parse_siv(u32 siv, struct gbw_siv_info *out);

#endif
