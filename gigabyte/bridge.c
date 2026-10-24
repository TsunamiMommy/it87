// SPDX-License-Identifier: GPL-2.0-or-later
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/string.h>
#include <asm/processor.h>
#include <asm/intel-family.h>

#include "bridge.h"

/* Defines fallbacks for processor models */
#ifndef INTEL_SKYLAKE_L_MODEL
#define INTEL_SKYLAKE_L_MODEL  0x4E
#endif
#ifndef INTEL_SKYLAKE_MODEL
#define INTEL_SKYLAKE_MODEL    0x5E
#endif
#ifndef INTEL_SKYLAKE_X_MODEL
#define INTEL_SKYLAKE_X_MODEL  0x55
#endif
#ifndef INTEL_KABYLAKE_L_MODEL
#define INTEL_KABYLAKE_L_MODEL 0x8E
#endif
#ifndef INTEL_KABYLAKE_MODEL
#define INTEL_KABYLAKE_MODEL   0x9E
#endif

/* Hidden window offsets by Intel PCH generation */
#define IT87_HIDDEN_OFS_SKYLAKE         0x00EF2700u
#define IT87_HIDDEN_OFS_Z390            0x00882700u
/* Z390 fixed hidden base fallback used when 00:1f.1 BAR0 is unavailable */
#define IT87_HIDDEN_BASE_Z390_FALLBACK  0xFD882700u

/* Defines vendor ID's for PCI to ISA bridges */
#define IT87_H2_VENDOR_AMD     0x1022
#define IT87_H2_VENDOR_INTEL   0x8086

/* PCI Read Routine */
static inline int pci_reg_read(struct pci_dev *d, u16 off, u32 *v)
{
	return pci_read_config_dword(d, off, v);
}

/* PCI Write Routine */
static inline int pci_reg_write(struct pci_dev *d, u16 off, u32 v)
{
	return pci_write_config_dword(d, off, v);
}

DEFINE_MUTEX(gigabyte_bridge_mutex);

/* MMIO PCI to ISA Bridge management struct */
struct it87_h2ram_handle
{
	struct pci_dev *bridge;
	bool            is_amd;
	bool            is_intel;
	bool            intel_espi;
	u8  		    intel_isabridge_type; /* General/z390/skylake */
	/* Saved dwords we modify (captured once, restored in quiesce/release) */
	u32 or48, or60, or6c;     	/* AMD: 0x48 MMIO port enable, 0x60 range, 0x6C ROM range 2 */
	u32 ord8, or98;          	/* Intel: 0xD8 Bridge enable address, 0x98 MMIO Base address */
	u32 hidden_orig_0x44; 	/* Original mirror D8 at hidden_base+0x44 */
	u32 hidden_orig_0x40; 	/* Original mirror 98 at hidden_base+0x40 */
	bool saved;				/* original PCI bridge registers are valid */
	bool hidden_saved;		/* original Intel hidden mirror is valid */

	/* Per-slot requested windows (idx 0=2E, 1=4E), 64KiB aligned */
	u32 base[2];
	/* AMD per slot precalculated registers */
	u32 r48[2];
	u32 r60[2];
	u32 r6c[2];
	/* Intel per slot precalculated registers */
	u32 rd8[2];
	u32 r98[2];
	bool have[2];

	u32 hidden_base;   		/* hidden base address for z390/skylake bridges */
	bool hidden_ready;       /* hidden window ready/available */
	/* AMD/Intel: track currently programmed base to minimize churn */
	u32 current_base;
};

/* Global MMIO bridge state tracking */
static struct it87_h2ram_handle it87_h2_global;
static bool                    it87_h2_global_ready;
/* Only call gigabyte_bridge_init() once from sm_it87_init() */
static bool                    it87_h2_global_inited;

/*
 * Intel ISA bridge types:
 * - Z390 bridge when Gigabyte platform id (SIV) == 4 or 6
 * - Else Skylake ISA bridge for Intel Skylake/Kaby/Coffee families
 * - Else generic Intel ISA bridge
 */
enum it87_isabridge_type {
	IT87_ISA_UNKNOWN       = 0,
	IT87_ISA_INTEL_GENERIC = 1,
	IT87_ISA_INTEL_SKYLAKE = 2,
	IT87_ISA_INTEL_Z390    = 3,
};

/* ==== BEGIN: Global H2RAM / ISA-bridge MMIO manager and hybrid accessors ==== */

/* checks for compatible skylake bridges */
static bool cpu_is_skl_kbl_cfl_family(void)
{
	const struct cpuinfo_x86 *c = &boot_cpu_data;

	if (c->x86_vendor != X86_VENDOR_INTEL) {
		return false;
	}

	if (c->x86 != 6) {
		return false;
	}

	switch (c->x86_model) {
	case INTEL_SKYLAKE_L_MODEL:
	case INTEL_SKYLAKE_MODEL:
	case INTEL_SKYLAKE_X_MODEL:    /* 0x55 bucket */
	case INTEL_KABYLAKE_L_MODEL:   /* includes Coffee Lake client buckets */
	case INTEL_KABYLAKE_MODEL:     /* includes Coffee Lake client buckets */
		return true;

	default:
		return false;
	}
}

/* Detect platform and compute/capture hidden base and
 * originals, unlocking E1=0x10 as needed. Caches kind, base, and originals
 * into 'h'. For generic Intel (no hidden window), marks hidden_ready=false. */
static int it87_intel_init_hidden(struct it87_h2ram_handle *h)
{
	struct pci_bus *bus;
	unsigned int pch_f1 = PCI_DEVFN(0x1f, 1);
	u32 bar0 = 0;
	u8 e1 = 0;
	bool e1_changed = false;
	int ret = 0;
	u32 hidden_ofs = 0;
	u8 platform = 0;
	int siv_ret;

	if (!h)
		return -EINVAL;

	h->hidden_ready = false;

	/*
	 * Select the PCR port ID used by this PCH generation.  Platform IDs
	 * 4 and 6 use the newer DMI port; earlier client generations use the
	 * Skylake-family DMI port.  Other Intel bridges need no PCR mirror.
	 */
	siv_ret = gigabyte_bridge_platform_id(&platform);
	if (siv_ret == 0 && (platform == 4 || platform == 6)) {
		h->intel_isabridge_type = IT87_ISA_INTEL_Z390;
		hidden_ofs = IT87_HIDDEN_OFS_Z390;
	} else if (cpu_is_skl_kbl_cfl_family()) {
		h->intel_isabridge_type = IT87_ISA_INTEL_SKYLAKE;
		hidden_ofs = IT87_HIDDEN_OFS_SKYLAKE;
	} else {
		h->intel_isabridge_type = IT87_ISA_INTEL_GENERIC;
		h->hidden_base = 0;
		h->hidden_ready = false;
		return 0;
	}

	/*
	 * Address P2SB through the root bus config-space operations.  P2SB is
	 * normally hidden before PCI enumeration, so looking it up as a normal
	 * pci_dev would fail even though its configuration space still responds.
	 */
	bus = pci_find_bus(0, 0);
	if (!bus)
		goto fallback;

	/* Save the function-hide control so visibility can be restored exactly. */
	ret = pci_bus_read_config_byte(bus, pch_f1, 0xE1, &e1);
	if (ret)
		goto fallback;
	if (e1 != 0x10) {
		/* Clear the hide state while retaining the required control bits. */
		ret = pci_bus_write_config_byte(bus, pch_f1, 0xE1, 0x10);
		if (ret)
			return -EIO;
		usleep_range(1000, 2000);
		e1_changed = true;
	}

	/* BAR0 supplies the base of the PCH private sideband register aperture. */
	ret = pci_bus_read_config_dword(bus, pch_f1, 0x10, &bar0);
	if (ret) {
		ret = -EIO;
		goto restore_e1;
	}
	if (!bar0 || bar0 == 0xFFFFFFFFu) {
		if (hidden_ofs == IT87_HIDDEN_OFS_Z390) {
			h->hidden_base = IT87_HIDDEN_BASE_Z390_FALLBACK;
			h->hidden_ready = true;
			goto restore_e1;
		}
		ret = -ENODEV;
		goto restore_e1;
	}
	/* Add the generation-specific DMI PCR port and register-block offset. */
	h->hidden_base = (bar0 & 0xFF000000u) + hidden_ofs;
	h->hidden_ready = true;

restore_e1:
	/* Return P2SB to the exact visibility state observed on entry. */
	if (e1_changed) {
		if (pci_bus_write_config_byte(bus, pch_f1, 0xE1, e1)) {
			h->hidden_base = 0;
			h->hidden_ready = false;
			return -EIO;
		}
		usleep_range(1000, 2000);
	}
	return ret;

fallback:
	/* Z390 firmware uses a fixed sideband aperture when BAR0 is unavailable. */
	if (hidden_ofs == IT87_HIDDEN_OFS_Z390) {
		h->hidden_base = IT87_HIDDEN_BASE_Z390_FALLBACK;
		h->hidden_ready = true;
		return 0;
	}
	return -ENODEV;
}

/* ----- Intel BIOS Data/Feature mask helpers ----- */

static u16 _intel_bios_mask_for_data_space(u32 base)
{
	if ((base & ~0xFFFFF) == 0xFF400000) return 0x0001;
	if ((base & ~0xFFFFF) == 0xFF500000) return 0x0002;
	if ((base & ~0xFFFFF) == 0xFF600000) return 0x0004;
	if ((base & ~0xFFFFF) == 0xFF700000) return 0x0008;
	if ((base & ~0xFFFF)  == 0x000E0000) return 0x0040;
	if ((base & ~0xFFFF)  == 0x000F0000) return 0x0080;
	if ((base & ~0x7FFFF) == 0xFFC00000) return 0x0100;
	if ((base & ~0x7FFFF) == 0xFFC80000) return 0x0200;
	if ((base & ~0x7FFFF) == 0xFFD00000) return 0x0400;
	if ((base & ~0x7FFFF) == 0xFFD80000) return 0x0800;
	if ((base & ~0x7FFFF) == 0xFFE00000) return 0x1000;
	if ((base & ~0x7FFFF) == 0xFFE80000) return 0x2000;
	if ((base & ~0x7FFFF) == 0xFFF00000) return 0x4000;
	if ((base & ~0x7FFFF) == 0xFFF80000) return 0x8000;
	return 0;
}

static u16 _intel_bios_mask_for_feat_space(u32 base)
{
	if ((base & ~0xFFFFF) == 0xFF000000) return 0x0001;
	if ((base & ~0xFFFFF) == 0xFF100000) return 0x0002;
	if ((base & ~0xFFFFF) == 0xFF200000) return 0x0004;
	if ((base & ~0xFFFFF) == 0xFF300000) return 0x0008;
	if ((base & ~0x7FFFF) == 0xFF800000) return 0x0100;
	if ((base & ~0x7FFFF) == 0xFF880000) return 0x0200;
	if ((base & ~0x7FFFF) == 0xFF900000) return 0x0400;
	if ((base & ~0x7FFFF) == 0xFF980000) return 0x0800;
	if ((base & ~0x7FFFF) == 0xFFA00000) return 0x1000;
	if ((base & ~0x7FFFF) == 0xFFA80000) return 0x2000;
	if ((base & ~0x7FFFF) == 0xFFB00000) return 0x4000;
	if ((base & ~0x7FFFF) == 0xFFB80000) return 0x8000;
	return 0;
}

/* ----- internal save/restore of original bridge state ----- */

static int _save_regs(struct it87_h2ram_handle *h)
{
	void __iomem *hb;
	int ret;
	u16 v;

	if (!h || !h->bridge)
		return -EINVAL;
	if (h->saved)
		return 0;

	v = h->bridge->vendor;
	if (v == IT87_H2_VENDOR_AMD) {
		ret = pci_reg_read(h->bridge, 0x48, &h->or48);
		if (ret)
			return ret;
		ret = pci_reg_read(h->bridge, 0x60, &h->or60);
		if (ret)
			return ret;
		ret = pci_reg_read(h->bridge, 0x6C, &h->or6c);
		if (ret)
			return ret;
	} else if (v == IT87_H2_VENDOR_INTEL) {
		/* Snapshot the public bridge decode registers before taking ownership. */
		ret = pci_reg_read(h->bridge, 0xD8, &h->ord8);
		if (ret)
			return ret;
		ret = pci_reg_read(h->bridge, 0x98, &h->or98);
		if (ret)
			return ret;

		if (h->hidden_ready && h->hidden_base) {
			/* Snapshot the matching PCR copies for rollback and module unload. */
			hb = ioremap(h->hidden_base, 0x200);
			if (!hb)
				return -ENOMEM;

			h->hidden_orig_0x40 = readl(hb + 0x40);
			h->hidden_orig_0x44 = readl(hb + 0x44);
			h->hidden_saved = true;
			iounmap(hb);
		}
	} else {
		return -ENODEV;
	}

	h->saved = true;
	return 0;
}

static void _restore_regs(struct it87_h2ram_handle *h)
{
	void __iomem *hb;
	int ret;
	u16 v;

	if (!h || !h->bridge || !h->saved)
		return;

	v = h->bridge->vendor;
	if (v == IT87_H2_VENDOR_AMD) {
		ret = pci_reg_write(h->bridge, 0x48, h->or48);
		if (ret)
			pr_warn("failed to restore AMD bridge register 0x48: %d\n", ret);
		ret = pci_reg_write(h->bridge, 0x60, h->or60);
		if (ret)
			pr_warn("failed to restore AMD bridge register 0x60: %d\n", ret);
		ret = pci_reg_write(h->bridge, 0x6C, h->or6c);
		if (ret)
			pr_warn("failed to restore AMD bridge register 0x6c: %d\n", ret);
	} else if (v == IT87_H2_VENDOR_INTEL) {
		/* Mirror hidden first, then PCI config */
		if (h->hidden_saved && h->hidden_base) {
			hb = ioremap(h->hidden_base, 0x200);
			if (hb) {
				writel(h->hidden_orig_0x40, hb + 0x40);
				writel(h->hidden_orig_0x44, hb + 0x44);
				/* Flush posted writes before the mapping is dropped. */
				(void)readl(hb + 0x44);
				iounmap(hb);
			} else {
				pr_warn("unable to map Intel hidden bridge window while restoring state\n");
			}
		}
		ret = pci_reg_write(h->bridge, 0xD8, h->ord8);
		if (ret)
			pr_warn("failed to restore Intel bridge register 0xd8: %d\n", ret);
		ret = pci_reg_write(h->bridge, 0x98, h->or98);
		if (ret)
			pr_warn("failed to restore Intel bridge register 0x98: %d\n", ret);
	}

	/* Never trust the cached slot after a restore or rollback. */
	h->current_base = 0;
}

/* ----- discrete per-slot programming ----- */

/* AMD:
 *  slot 0 (2E): START=(base>>16)&0xFF00; END=START+1;
 *               0x60=(END<<16)|START
 *               0x6C=0xFFFFFF00
 *               0x48: set bit5
 *  slot 1 (4E): START=(base>>16)&0xFFFF; END=START+1;
 *               0x60=(END<<16)|START
 *               0x6C=(0xFFFF0000 | END)
 *               0x48: set bit5
 */
static int _amd_enable_slot(struct it87_h2ram_handle *h, int idx)
{
	int ret;

	if (!h || !h->bridge)
		return -ENODEV;
	if (idx < 0 || idx > 1 || !h->have[idx])
		return -EINVAL;

	ret = pci_reg_write(h->bridge, 0x60, h->r60[idx]);
	if (ret)
		goto rollback;
	ret = pci_reg_write(h->bridge, 0x6C, h->r6c[idx]);
	if (ret)
		goto rollback;
	ret = pci_reg_write(h->bridge, 0x48, h->r48[idx]);
	if (ret)
		goto rollback;

	h->current_base = h->base[idx];
	return 0;

rollback:
	_restore_regs(h);
	return ret;
}

/* Intel:
 *  0x98: (START<<16)|1 with START=(base>>16)
 *  0xD8: clear enable bit(s) (active-low):
 *        - slot0: clear bit0
 *        - slot1: clear one bit chosen from address tables
 */
static int _intel_enable_slot(struct it87_h2ram_handle *h, int idx)
{
	void __iomem *hb;
	u32 verify;
	int ret;

	if (!h || !h->bridge)
		return -ENODEV;
	if (idx < 0 || idx > 1 || !h->have[idx])
		return -EINVAL;

	if (h->current_base == h->base[idx])
		return 0; /* already active */

	/*
	 * Program the private DMI PCR copies first.  Until these mirrors agree,
	 * writes to the public bridge registers may not affect forwarded cycles.
	 */
	if (h->hidden_ready) {
		hb = ioremap(h->hidden_base, 0x200);
		if (!hb)
			return -ENOMEM;

		writel(h->r98[idx], hb + 0x40);
		writel(h->rd8[idx], hb + 0x44);
		/* Read back both dwords, also flushing any posted MMIO writes. */
		if (readl(hb + 0x40) != h->r98[idx] ||
		    readl(hb + 0x44) != h->rd8[idx]) {
			iounmap(hb);
			ret = -EIO;
			goto rollback;
		}
		iounmap(hb);
	}

	/* Publish the same decode values through the visible ISA bridge. */
	ret = pci_reg_write(h->bridge, 0xD8, h->rd8[idx]);
	if (ret)
		goto rollback;

	ret = pci_reg_write(h->bridge, 0x98, h->r98[idx]);
	if (ret)
		goto rollback;
	/* Confirm both public registers accepted the requested decode state. */
	ret = pci_reg_read(h->bridge, 0xD8, &verify);
	if (ret || verify != h->rd8[idx]) {
		ret = ret ? ret : -EIO;
		goto rollback;
	}
	ret = pci_reg_read(h->bridge, 0x98, &verify);
	if (ret || verify != h->r98[idx]) {
		ret = ret ? ret : -EIO;
		goto rollback;
	}

	h->current_base = h->base[idx];
	return 0;

rollback:
	/* Any partial update is unsafe, so restore all saved public and PCR state. */
	_restore_regs(h);
	return ret;
}

static int _enable_slot(struct it87_h2ram_handle *h, int idx)
{
	u16 v;

	if (!h || !h->bridge)return -ENODEV;
	v = h->bridge->vendor;
	if (v==IT87_H2_VENDOR_AMD)return 	_amd_enable_slot(h, idx);
	if (v==IT87_H2_VENDOR_INTEL)return 	_intel_enable_slot(h, idx);
	return -ENODEV;
}

/* ----- compact internal API (per-bridge handle) ----- */

static int it87_h2_init(struct it87_h2ram_handle *h)
{
	struct pci_dev *pdev;
	unsigned int devfn;
	u16 vendor;
	int ret;

	if (!h)
		return -EINVAL;

	memset(h, 0, sizeof(*h));

	switch (boot_cpu_data.x86_vendor) {
	case X86_VENDOR_INTEL:
		vendor = IT87_H2_VENDOR_INTEL;
		devfn = PCI_DEVFN(0x1f, 0);
		break;
	case X86_VENDOR_AMD:
		vendor = IT87_H2_VENDOR_AMD;
		devfn = PCI_DEVFN(0x14, 3);
		break;
	default:
		return -ENODEV;
	}

	pdev = pci_get_domain_bus_and_slot(0, 0, devfn);
	if (!pdev)
		return -ENODEV;
	if (pdev->vendor != vendor) {
		pci_dev_put(pdev);
		return -ENODEV;
	}

	h->bridge = pdev;
	ret = pci_enable_device(h->bridge);
	if (ret)
		goto err_put_bridge;

	h->is_amd = h->bridge->vendor == IT87_H2_VENDOR_AMD;
	h->is_intel = h->bridge->vendor == IT87_H2_VENDOR_INTEL;

	if (h->is_intel) {
		u32 interface;

		/* D31:F0 0xdc bit 2 selects eSPI instead of LPC. */
		if (!pci_reg_read(h->bridge, 0xdc, &interface))
			h->intel_espi = !!(interface & BIT(2));

		ret = it87_intel_init_hidden(h);
		if (ret < 0)
			goto err_disable_bridge;
	}

	ret = _save_regs(h);
	if (ret)
		goto err_disable_bridge;

	return 0;

err_disable_bridge:
	pci_disable_device(h->bridge);
err_put_bridge:
	pci_dev_put(h->bridge);
	h->bridge = NULL;
	return ret;
}

/* Set up MMIO bridge register values */
static int it87_h2_set_slot(struct it87_h2ram_handle *h, int idx, u64 mmio_base)
{
	u32 base32;
	u16 mask;

	if (!h || !h->bridge)return -ENODEV;
	if (idx<0 || idx>1)return -EINVAL;
	if (mmio_base==0)return -EINVAL;
	if (mmio_base > 0xFFFFFFFFull)return -ERANGE;

	/* The bridge window is encoded at 64 KiB granularity. */
	base32 = (u32)mmio_base;
	base32 &= ~0xFFFFu;                        /* 64KiB align down */

	/* If bridge is amd calculate the register values for the bridge window of idx */
	if (h->bridge->vendor == IT87_H2_VENDOR_AMD) {
		if (idx == 1) {
			h->r48[idx] = (h->or48 & ~BIT(5)) | BIT(5);
			h->r60[idx] = ((((base32 >> 16) & 0xFFFFu) + 1u) << 16) | ((base32 >> 16) & 0xFFFFu);
			h->r6c[idx] = 0xFFFF0000u | (((base32 >> 16) & 0xFFFFu) + 1u);
		} else {
			h->r48[idx] = (h->or48 & ~BIT(5)) | BIT(5);
			h->r60[idx] = (((base32 >> 16) & 0xFF00u) + 1u) << 16 | ((base32 >> 16) & 0xFF00u);
			h->r6c[idx] = 0xFFFFFF00u;
		}
	/* If bridge is intel calculate the register values for the bridge window of idx */
	} else if (h->bridge->vendor == IT87_H2_VENDOR_INTEL) {
		/*
		 * The primary 2E/2F controller always uses decode bit 0.  The
		 * secondary 4E/4F controller selects an active-low BIOS-range bit
		 * from its physical address.
		 */
		mask = idx == 0 ? BIT(0) :
			_intel_bios_mask_for_data_space(base32);
		if (!mask && idx == 1)
			mask = _intel_bios_mask_for_feat_space(base32);
		if (!mask)
			return -EINVAL;
		/* Bit 0 enables the generic memory range; clearing D8 routes it. */
		h->r98[idx] = base32 | 1u;
		h->rd8[idx] = h->ord8 & ~(u32)mask;
	} else {
		return -ENODEV;
	}

	/* Commit the slot only after every vendor-specific value is valid. */
	h->base[idx] = base32;
	h->have[idx] = true;

	return 0;
}

static int it87_h2_use_slot(struct it87_h2ram_handle *h, int idx)
{
	if (!h || !h->bridge)return -ENODEV;
	if (idx<0 || idx>1)return -EINVAL;
	if (!h->have[idx])return -ENOENT;

	/* Program window on demand for all vendors */
	if (h->current_base != h->base[idx]) {
		return _enable_slot(h, idx);
	}
	return 0;
}

static void it87_h2_release(struct it87_h2ram_handle *h)
{
	if (!h || !h->bridge)
		return;

	_restore_regs(h);
	pci_disable_device(h->bridge);
	pci_dev_put(h->bridge);
	memset(h, 0, sizeof(*h));
}

/* ----- Global, locked API for shared MMIO bridge ----- */

/* Called once from sm_it87_init(), after at least one it87_find()
 * reported a valid mmio_address + mmio_bridge/mmio_h2ram.
 */
int gigabyte_bridge_init(void)
{
	int ret;
	ret = it87_h2_init(&it87_h2_global);
	if (!ret) {
		it87_h2_global_ready = true;
		it87_h2_global_inited = true;
	}
	return ret;
}

/* Configure a slot (just updates state, does not touch PCI yet) */
int gigabyte_bridge_set_slot(int idx, u64 mmio_base)
{
	int ret;

	mutex_lock(&gigabyte_bridge_mutex);
	if (!it87_h2_global_ready)
		ret = -ENODEV;
	else
		ret = it87_h2_set_slot(&it87_h2_global, idx, mmio_base);
	mutex_unlock(&gigabyte_bridge_mutex);

	return ret;
}

/* Ensure a specific slot is active (AMD may reprogram bridge) */
int gigabyte_bridge_use_slot(int idx)
{
	int ret;
	if (!it87_h2_global_ready) {
		return -ENODEV;
	}
	ret = it87_h2_use_slot(&it87_h2_global, idx);
	return ret;
}

/* Firmware/PCI resume may have changed the bridge behind our cache. */
void gigabyte_bridge_invalidate(void)
{
	mutex_lock(&gigabyte_bridge_mutex);
	if (it87_h2_global_ready)
		it87_h2_global.current_base = 0;
	mutex_unlock(&gigabyte_bridge_mutex);
}

/* Fully release: restore PCI config, disable the bridge, and drop its ref. */
void gigabyte_bridge_release(void)
{
	mutex_lock(&gigabyte_bridge_mutex);
	if (it87_h2_global.bridge)
		it87_h2_release(&it87_h2_global);

	it87_h2_global_ready  = false;
	it87_h2_global_inited = false;
	mutex_unlock(&gigabyte_bridge_mutex);
}

/* ==== END: Global H2RAM / ISA-bridge MMIO manager and hybrid accessors ==== */

bool gigabyte_bridge_ready(void)
{
	return it87_h2_global_ready;
}

bool gigabyte_bridge_initialized(void)
{
	return it87_h2_global_inited;
}

bool gigabyte_bridge_intel_espi(void)
{
	return it87_h2_global.intel_espi;
}
