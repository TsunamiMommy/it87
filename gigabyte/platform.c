// SPDX-License-Identifier: GPL-2.0-or-later
#include <linux/errno.h>
#include <linux/string.h>

#include "platform.h"

int gigabyte_platform_parse_siv(u32 siv, struct gbw_siv_info *out)
{
	if (!out)
		return -EINVAL;

	memset(out, 0, sizeof(*out));
	out->siv = siv;
	if (!siv)
		return -ENODEV;

	out->platform = (siv >> 28) & 0x0f;
	if (!out->platform)
		return -ENODEV;

	out->special = (siv >> 24) & 0x0f;
	out->fan_count = (siv >> 16) & 0x1f;
	out->temp_count = (siv >> 8) & 0x1f;
	out->volt_count = siv & 0x1f;
	out->supported = true;
	return 0;
}
