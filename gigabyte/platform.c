// SPDX-License-Identifier: GPL-2.0-or-later
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/device.h>
#include <linux/dmi.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/sysfs.h>

#include "gigabyte.h"
#include "platform.h"

#define GIGABYTE_SMI_CMD_SIV_ID	0xb364
#define GIGABYTE_SMI_CMD_LED_ID	0xb464

static u64 gigabyte_siv;
static u64 gigabyte_lid;
static bool gigabyte_siv_valid;
static bool gigabyte_lid_valid;
static bool gigabyte_dmi_valid;
static struct class *gigabyte_class;
static struct device *gigabyte_dev;

static const struct dmi_system_id gigabyte_dmi_table[] = {
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR,
				  "Gigabyte Technology Co., Ltd."),
		},
	},
	{ }
};

/* The decoded Gigabyte firmware identity is shared with its backends. */

int gigabyte_platform_get_siv(u32 *siv)
{
	if (!siv)
		return -EINVAL;
	if (!gigabyte_siv_valid)
		return -ENODEV;

	*siv = (u32)gigabyte_siv;
	return 0;
}

/* Read the cached SIV and parse it. */
int gigabyte_platform_read_siv_info(struct gbw_siv_info *out)
{
	u32 siv;
	int ret = gigabyte_platform_get_siv(&siv);
	if (ret)
		return ret;
	return gigabyte_platform_parse_siv(siv, out);
}

/*
 * All Gigabyte-specific MMIO bridge, H2RAM and firmware-control workarounds
 * require both a Gigabyte DMI match and a successfully read, parseable SIV.
 * Native IT87 MMIO support is intentionally not covered by this predicate.
 */
bool gigabyte_platform_valid(void)
{
	struct gbw_siv_info info;

	return gigabyte_dmi_valid && !gigabyte_platform_read_siv_info(&info) && info.supported;
}

bool gigabyte_platform_has_siv(void)
{
	return gigabyte_dmi_valid && gigabyte_siv_valid;
}

/* Convenience getters for individual SIV fields */
int gigabyte_platform_id(u8 *platform)
{
	struct gbw_siv_info info;
	int ret = gigabyte_platform_read_siv_info(&info);
	if (ret)
		return ret;
	*platform = info.platform;
	return 0;
}

int gigabyte_platform_fan_count(u8 *count)
{
	struct gbw_siv_info info;
	int ret = gigabyte_platform_read_siv_info(&info);
	if (ret)
		return ret;
	*count = info.fan_count;
	return 0;
}

#ifdef IT87_FUTURE_USE
static int gbw_siv_temp_count(u8 *count)
{
	struct gbw_siv_info info;
	int ret = gigabyte_platform_read_siv_info(&info);
	if (ret)
		return ret;
	*count = info.temp_count;
	return 0;
}
#endif

#ifdef IT87_FUTURE_USE
static int gbw_siv_volt_count(u8 *count)
{
	struct gbw_siv_info info;
	int ret = gigabyte_platform_read_siv_info(&info);
	if (ret)
		return ret;
	*count = info.volt_count;
	return 0;
}
#endif
/* End of Gigabyte SIV/LID retrieval routines */

static ssize_t gigabyte_siv_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	if (!gigabyte_siv_valid)
		return sprintf(buf, "unavailable\n");

	return sprintf(buf, "%08llX\n", (unsigned long long)gigabyte_siv);
}

static ssize_t gigabyte_lid_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	if (!gigabyte_lid_valid)
		return sprintf(buf, "unavailable\n");

	return sprintf(buf, "%08llX\n", (unsigned long long)gigabyte_lid);
}

static DEVICE_ATTR_RO(gigabyte_siv);
static DEVICE_ATTR_RO(gigabyte_lid);

static struct attribute *gigabyte_id_attributes[] = {
	&dev_attr_gigabyte_siv.attr,
	&dev_attr_gigabyte_lid.attr,
	NULL
};

static const struct attribute_group gigabyte_id_group = {
	.attrs = gigabyte_id_attributes,
};

int __init gigabyte_platform_init(void)
{
	int ret;

	gigabyte_dmi_valid = dmi_check_system(gigabyte_dmi_table);
	if (!gigabyte_dmi_valid)
		return 0;

	gigabyte_siv_valid = !gigabyte_read_id(GIGABYTE_SMI_CMD_SIV_ID,
						       &gigabyte_siv);
	gigabyte_lid_valid = !gigabyte_read_id(GIGABYTE_SMI_CMD_LED_ID,
						       &gigabyte_lid);

	if (gigabyte_siv_valid)
		pr_info("Gigabyte SIV ID = %08llX\n",
			(unsigned long long)gigabyte_siv);
	else
		pr_warn("Failed to read Gigabyte SIV ID\n");

	if (gigabyte_lid_valid)
		pr_info("Gigabyte LID ID = %08llX\n",
			(unsigned long long)gigabyte_lid);
	else
		pr_warn("Failed to read Gigabyte LID ID\n");

	gigabyte_class = class_create("gigabyte");
	if (IS_ERR(gigabyte_class)) {
		ret = PTR_ERR(gigabyte_class);
		gigabyte_class = NULL;
		return ret;
	}

	gigabyte_dev = device_create(gigabyte_class, NULL, MKDEV(0, 0), NULL,
				      "id");
	if (IS_ERR(gigabyte_dev)) {
		ret = PTR_ERR(gigabyte_dev);
		gigabyte_dev = NULL;
		class_destroy(gigabyte_class);
		gigabyte_class = NULL;
		return ret;
	}

	ret = sysfs_create_group(&gigabyte_dev->kobj, &gigabyte_id_group);
	if (ret) {
		device_unregister(gigabyte_dev);
		gigabyte_dev = NULL;
		class_destroy(gigabyte_class);
		gigabyte_class = NULL;
	}

	return ret;
}

void gigabyte_platform_exit(void)
{
	if (gigabyte_dev) {
		sysfs_remove_group(&gigabyte_dev->kobj, &gigabyte_id_group);
		device_unregister(gigabyte_dev);
		gigabyte_dev = NULL;
	}

	if (gigabyte_class) {
		class_destroy(gigabyte_class);
		gigabyte_class = NULL;
	}

	gigabyte_siv_valid = false;
	gigabyte_lid_valid = false;
	gigabyte_dmi_valid = false;
}

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
