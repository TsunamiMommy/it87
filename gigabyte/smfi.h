/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef IT87_GIGABYTE_SMFI_H
#define IT87_GIGABYTE_SMFI_H

#include <linux/types.h>

#define GIGABYTE_SIO_H2RAM 0x0f
#define IT87_SMFI_ENABLE  0x30
#define IT87_SMFI_BASE_LOW 0xf5
#define IT87_SMFI_BASE_HI  0xf6
#define IT87_SMFI_BASE_EX  0xfc

void gigabyte_smfi_probe(int sioaddr, bool extended, bool can_ecio,
			 bool amd, phys_addr_t *base,
			 bool *mapped, bool *ecio);

#endif
