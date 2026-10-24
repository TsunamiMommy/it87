/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef IT87_TEST_BITOPS_H
#define IT87_TEST_BITOPS_H
#define BIT(n) (1UL << (n))
#define GENMASK(h, l) (((~0UL) << (l)) & (~0UL >> (sizeof(unsigned long) * 8 - 1 - (h))))
#endif
