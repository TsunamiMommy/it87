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
int gigabyte_platform_init(void);
void gigabyte_platform_exit(void);
bool gigabyte_platform_valid(void);
bool gigabyte_platform_has_siv(void);
int gigabyte_platform_get_siv(u32 *siv);
int gigabyte_platform_read_siv_info(struct gbw_siv_info *info);
int gigabyte_platform_id(u8 *platform);
int gigabyte_platform_fan_count(u8 *count);

#endif
