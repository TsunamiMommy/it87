// SPDX-License-Identifier: GPL-2.0-or-later
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "gigabyte/smartfan/smartfan.h"

#define REG_COUNT 0x1000

struct it87_data {
	u8 regs[REG_COUNT];
	int reads;
	int writes;
};

static int fake_read(struct it87_data *data, u16 reg)
{
	assert(reg < REG_COUNT);
	data->reads++;
	return data->regs[reg];
}

static void fake_write(struct it87_data *data, u16 reg, u8 val)
{
	assert(reg < REG_COUNT);
	data->regs[reg] = val;
	data->writes++;
}

static void test_detection(void)
{
	u8 ver, fans;

	assert(gigabyte_smartfan_detect_g23(true, true, 1, 0) ==
	       IT87_H2RAM_SF_G3);
	assert(gigabyte_smartfan_detect_g23(false, true, 1, 0) ==
	       IT87_H2RAM_SF_NONE);
	assert(gigabyte_smartfan_detect_g23(false, true, 0, 1) ==
	       IT87_H2RAM_SF_G2);
	assert(gigabyte_smartfan_detect_g23(false, true, 0, 3) ==
	       IT87_H2RAM_SF_G2);
	assert(gigabyte_smartfan_detect_g23(false, true, 0, 4) ==
	       IT87_H2RAM_SF_G2);
	assert(gigabyte_smartfan_detect_g23(false, true, 0, 2) ==
	       IT87_H2RAM_SF_NONE);
	assert(gigabyte_smartfan_detect_g23(true, true, 1, 1) ==
	       IT87_H2RAM_SF_NONE);
	assert(gigabyte_smartfan_detect_g1(true, 1, 1));
	assert(!gigabyte_smartfan_detect_g1(true, 1, 0));
	assert(!gigabyte_smartfan_detect_g1(false, 1, 1));

	ver = fans = 0;
	gigabyte_smartfan_it57xx_detect(1, 0, false, &ver, &fans);
	assert(ver == 1 && fans == 2);
	gigabyte_smartfan_it57xx_detect(2, 0, false, &ver, &fans);
	assert(ver == 2 && fans == 4);
	gigabyte_smartfan_it57xx_detect(2, 0, true, &ver, &fans);
	assert(ver == 2 && fans == 2);
	gigabyte_smartfan_it57xx_detect(3, 0, false, &ver, &fans);
	assert(ver == 2 && fans == 2);
	assert(gigabyte_smartfan_override_mask(IT87_H2RAM_SF_NONE, 0,
						0xff) == 0);
	assert(gigabyte_smartfan_override_mask(IT87_H2RAM_SF_G1, 0,
						0xff) == 0x07);
	assert(gigabyte_smartfan_override_mask(IT87_H2RAM_SF_G2, 0,
						0xff) == 0x07);
	assert(gigabyte_smartfan_override_mask(IT87_H2RAM_SF_G3, 4,
						0xff) == 0x7f);
}

static void test_global_state(enum it87_h2ram_smartfan_gen gen)
{
	struct it87_data data = { 0 };
	struct gigabyte_smartfan_io io = {
		.data = &data, .read = fake_read, .write = fake_write,
	};
	u8 original = 0;
	bool saved = false;

	data.regs[0x947] = 0xb2;
	gigabyte_smartfan_claim(&io, gen, &original, &saved);
	assert(saved && original == 0xb2);
	assert(data.regs[0x947] == (gen == IT87_H2RAM_SF_G1 ?
				       0xb2 : 0xb3));
	data.regs[0x947] = 0x46;
	gigabyte_smartfan_claim(&io, gen, &original, &saved);
	assert(original == 0xb2);
	gigabyte_smartfan_release(&io, original, &saved, false);
	assert(saved && data.regs[0x947] == 0xb2);
	data.regs[0x947] = 0x46;
	gigabyte_smartfan_release(&io, original, &saved, true);
	assert(!saved && data.regs[0x947] == 0xb2);
	gigabyte_smartfan_release(&io, original, &saved, true);
	assert(data.regs[0x947] == 0xb2);
}

static void test_extvec(int fan)
{
	struct it87_data data = { 0 };
	struct gigabyte_smartfan_io io = {
		.data = &data, .read = fake_read, .write = fake_write,
	};
	const u16 base = 0x212 + fan * 0x10;
	const u16 other = 0x31c + fan * 0x10;
	u8 before[REG_COUNT];
	u8 saved_data[4] = { 0 };
	bool saved = false;
	int i;

	for (i = 0; i < REG_COUNT; i++)
		data.regs[i] = (u8)((i * 13 + 19) & 0xff);
	memcpy(before, data.regs, sizeof(before));
	gigabyte_smartfan_extvec_disable(&io, fan, &saved, saved_data);
	assert(saved);
	assert(data.writes == 4);
	for (i = 0; i < 3; i++) {
		u16 reg = base + i * 4;

		assert(saved_data[i] == before[reg]);
		assert(data.regs[reg] == (before[reg] & 0x7f));
	}
	assert(saved_data[3] == before[other]);
	assert(data.regs[other] == (before[other] & 0xc0));

	/* A second disable must preserve the first snapshot. */
	gigabyte_smartfan_extvec_disable(&io, fan, &saved, saved_data);
	assert(saved_data[3] == before[other]);
	gigabyte_smartfan_extvec_restore(&io, fan, &saved, saved_data, false);
	assert(saved);
	assert(memcmp(data.regs, before, sizeof(before)) == 0);
	gigabyte_smartfan_extvec_disable(&io, fan, &saved, saved_data);
	gigabyte_smartfan_extvec_restore(&io, fan, &saved, saved_data, true);
	assert(!saved);
	assert(memcmp(data.regs, before, sizeof(before)) == 0);
}

int main(void)
{
	int i;

	test_detection();
	test_global_state(IT87_H2RAM_SF_G1);
	test_global_state(IT87_H2RAM_SF_G2);
	test_global_state(IT87_H2RAM_SF_G3);
	for (i = 0; i < 6; i++)
		test_extvec(i);
	puts("Smart Fan ownership tests passed");
	return 0;
}
