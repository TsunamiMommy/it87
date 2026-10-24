// SPDX-License-Identifier: GPL-2.0-or-later
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/device.h>
#include <linux/module.h>

#include "../platform.h"
#include "smartfan.h"

#define GIGABYTE_G23_MAJOR	0x900
#define GIGABYTE_G23_MINOR	0x901
#define GIGABYTE_G1_MAJOR	0xa00
#define GIGABYTE_G1_MINOR	0xa01
#define GIGABYTE_IT57_MAJOR	0xc00
#define GIGABYTE_IT57_MINOR	0xc01

void gigabyte_smartfan_probe(struct device *dev,
			     const struct gigabyte_smartfan_io *io,
			     const char *chip, u8 revision, bool ecio,
			     bool allow_g1, bool allow_g2, bool allow_g3,
			     struct gigabyte_smartfan_probe_result *result)
{
	u8 g23_major, g23_minor;
	u8 g1_major = 0, g1_minor = 0;
	u8 it57_major = 0, it57_minor = 0;
	u8 version_major, version_minor, smartfan;
	bool siv_valid = false;
	int channels;

	g23_major = io->read(io->data, GIGABYTE_G23_MAJOR);
	g23_minor = io->read(io->data, GIGABYTE_G23_MINOR);

	result->gen = gigabyte_smartfan_detect_g23(allow_g3, allow_g2,
						  g23_major, g23_minor);
	if (result->gen == IT87_H2RAM_SF_NONE) {
		g1_major = io->read(io->data, GIGABYTE_G1_MAJOR);
		g1_minor = io->read(io->data, GIGABYTE_G1_MINOR);
		if (gigabyte_smartfan_detect_g1(allow_g1, g1_major, g1_minor))
			result->gen = IT87_H2RAM_SF_G1;
	}
	if (result->gen == IT87_H2RAM_SF_NONE)
		return;

	/* IT57xx extends G3 only when the reported channel count permits it. */
	if (result->gen == IT87_H2RAM_SF_G3 &&
	    !gigabyte_platform_fan_count(&result->siv_fan_count)) {
		siv_valid = true;
		if (result->siv_fan_count > 8) {
			it57_major = io->read(io->data, GIGABYTE_IT57_MAJOR);
			it57_minor = io->read(io->data, GIGABYTE_IT57_MINOR);
			gigabyte_smartfan_it57xx_detect(it57_major, it57_minor,
						       ecio,
						       &result->it57xx_version,
						       &result->it57xx_fans);
		}
	}

	smartfan = io->read(io->data, GIGABYTE_SMARTFAN_ENABLE_REG);
	channels = gigabyte_smartfan_channel_count(result->gen,
						  result->it57xx_fans);
	if (channels)
		result->pwm_mode = (smartfan & 0x01) ? 2 : 1;

	if (result->gen == IT87_H2RAM_SF_G1) {
		version_major = g1_major;
		version_minor = g1_minor;
	} else {
		version_major = g23_major;
		version_minor = g23_minor;
	}

	if (result->gen == IT87_H2RAM_SF_G3 && siv_valid)
		dev_info(dev,
			 "H2RAM SmartFan: %s rev %02x, G%u v%02x.%02x, access=EC, H2RAM=%s, SmartFan=0x%02x, channels=%d, SIV fan count=%u\n",
			 chip, revision, result->gen, version_major, version_minor,
			 ecio ? "ECIO" : "MMIO", smartfan, channels,
			 result->siv_fan_count);
	else if (result->gen == IT87_H2RAM_SF_G3)
		dev_info(dev,
			 "H2RAM SmartFan: %s rev %02x, G%u v%02x.%02x, access=EC, H2RAM=%s, SmartFan=0x%02x, channels=%d, SIV fan count=unavailable\n",
			 chip, revision, result->gen, version_major, version_minor,
			 ecio ? "ECIO" : "MMIO", smartfan, channels);
	else
		dev_info(dev,
			 "H2RAM SmartFan: %s rev %02x, G%u v%02x.%02x, access=EC, H2RAM=%s, SmartFan=0x%02x, channels=%d\n",
			 chip, revision, result->gen, version_major, version_minor,
			 ecio ? "ECIO" : "MMIO", smartfan, channels);

	if (result->gen == IT87_H2RAM_SF_G3 && siv_valid &&
	    result->siv_fan_count > 8) {
		if (result->it57xx_fans)
			dev_info(dev,
				 "IT57xx extension: v%02x.%02x, %u usable channels%s\n",
				 it57_major, it57_minor, result->it57xx_fans,
				 ecio && result->it57xx_version == 2 ?
				 " (ECIO limit)" : "");
		else
			dev_info(dev,
				 "IT57xx extension: unrecognized v%02x.%02x (SIV fan count %u)\n",
				 it57_major, it57_minor, result->siv_fan_count);
	}
}
