// SPDX-License-Identifier: GPL-2.0-or-later
#include <linux/types.h>
#include <asm/processor.h>

#include "gigabyte.h"

bool gigabyte_noise_siv_supported(u32 siv)
{
	if (boot_cpu_data.x86_vendor == X86_VENDOR_AMD) {
		switch (siv) {
		case 0x4308090b:
		case 0x4208090b:
		case 0x4108090b:
		case 0x4008090a:
		case 0x6108090b:
		case 0x800a090a:
		case 0x8108090a:
		case 0x5108090b:
		case 0x5208090b:
		case 0x5308090a:
		case 0x9a0a0908:
		case 0x9a0b0908:
		case 0x910a0909:
		case 0x900a0909:
		case 0x90090909:
		case 0x90080909:
		case 0xa00c090a:
		case 0xa00c0908:
		case 0xa00b090a:
		case 0xa00a090a:
		case 0xa10a090a:
		case 0xa20a090a:
		case 0xa00a0908:
		case 0xa008090a:
		case 0xa108090a:
		case 0xa208090a:
		case 0xa308090a:
		case 0xa0080908:
		case 0xa3080908:
			return true;
		default:
			return false;
		}
	}

	if (boot_cpu_data.x86_vendor == X86_VENDOR_INTEL) {
		switch (siv) {
		case 0x5008090a:
		case 0x5108090a:
		case 0x6108090a:
		case 0x6208090b:
		case 0x6508090b:
		case 0x710a090a:
		case 0x710a090b:
		case 0x720a090b:
		case 0x730a090b:
		case 0x7108090b:
		case 0x7208090b:
		case 0x7308090b:
		case 0x7106090b:
		case 0x800a0909:
		case 0x800a090a:
		case 0x810a090a:
		case 0x8008090a:
		case 0x8108090a:
		case 0x8008090b:
		case 0x900a0909:
		case 0x910a090a:
		case 0x900a090a:
		case 0x9108090a:
		case 0x9008090a:
		case 0x9008090b:
		case 0xa00b090b:
		case 0xa00a090b:
		case 0xa10a090b:
		case 0xa009090b:
		case 0xa008090b:
		case 0xa108090b:
		case 0xa00a080b:
			return true;
		default:
			return false;
		}
	}

	return false;
}
