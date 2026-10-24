/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef IT87_GIGABYTE_H
#define IT87_GIGABYTE_H

#include <linux/types.h>

int gigabyte_read_id(u16 cmd, u64 *id);
bool gigabyte_noise_siv_supported(u32 siv);

#endif
