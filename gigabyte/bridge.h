/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef IT87_GIGABYTE_BRIDGE_H
#define IT87_GIGABYTE_BRIDGE_H

#include <linux/mutex.h>
#include <linux/types.h>

extern struct mutex gigabyte_bridge_mutex;
bool gigabyte_bridge_ready(void);
bool gigabyte_bridge_initialized(void);
bool gigabyte_bridge_intel_espi(void);
int gigabyte_bridge_init(void);
int gigabyte_bridge_set_slot(int idx, u64 mmio_base);
int gigabyte_bridge_use_slot(int idx);
void gigabyte_bridge_invalidate(void);
void gigabyte_bridge_release(void);

#endif
