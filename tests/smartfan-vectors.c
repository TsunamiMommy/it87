// SPDX-License-Identifier: GPL-2.0-or-later
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "gigabyte/smartfan/smartfan.h"

#define REG_COUNT 0x1000
#define MAX_WRITES 64

struct write_record {
	u16 reg;
	u8 value;
};

struct it87_data {
	u8 regs[REG_COUNT];
	struct write_record writes[MAX_WRITES];
	int count;
	int reads;
};

static int fake_read(struct it87_data *data, u16 reg)
{
	assert(reg < REG_COUNT);
	data->reads++;
	return data->regs[reg];
}

static void fake_write(struct it87_data *data, u16 reg, u8 value)
{
	assert(reg < REG_COUNT);
	assert(data->count < MAX_WRITES);
	data->writes[data->count++] = (struct write_record) { reg, value };
	data->regs[reg] = value;
}

static void expect_write(struct it87_data *data, int pos, u16 reg, u8 value)
{
	assert(pos < data->count);
	assert(data->writes[pos].reg == reg);
	assert(data->writes[pos].value == value);
}

static void test_layout(void)
{
	u16 lsb, msb;
	int i;

	assert(gigabyte_smartfan_channel_count(IT87_H2RAM_SF_NONE, 0) == 0);
	assert(gigabyte_smartfan_channel_count(IT87_H2RAM_SF_G1, 0) == 0);
	assert(gigabyte_smartfan_channel_count(IT87_H2RAM_SF_G2, 4) == 3);
	assert(gigabyte_smartfan_channel_count(IT87_H2RAM_SF_G3, 2) == 5);
	assert(gigabyte_smartfan_channel_count(IT87_H2RAM_SF_G3, 4) == 7);

	for (i = 0; i < 3; i++) {
		assert(gigabyte_smartfan_vector_base(IT87_H2RAM_SF_G2, 0, i) ==
		       (u16)(0x950 + i * 0x10));
		assert(gigabyte_smartfan_vector_base(IT87_H2RAM_SF_G3, 0, i) ==
		       (u16)(0x950 + i * 0x30));
		assert(gigabyte_smartfan_tach_regs(IT87_H2RAM_SF_G3, 0, 0,
						   i, &lsb, &msb));
		assert(lsb == 0x916 + i * 2);
		assert(msb == 0x917 + i * 2);
	}

	assert(gigabyte_smartfan_vector_base(IT87_H2RAM_SF_G2, 0, 3) == 0);
	assert(gigabyte_smartfan_vector_base(IT87_H2RAM_SF_G3, 2, -1) == 0);
	assert(gigabyte_smartfan_vector_base(IT87_H2RAM_SF_G3, 2, 5) == 0);
	assert(!gigabyte_smartfan_tach_regs(IT87_H2RAM_SF_G2, 0, 0,
						    0, &lsb, &msb));
	assert(!gigabyte_smartfan_tach_regs(IT87_H2RAM_SF_G3, 2, 2,
						    5, &lsb, &msb));

	for (i = 0; i < 4; i++) {
		assert(gigabyte_smartfan_vector_base(IT87_H2RAM_SF_G3, 4,
							  i + 3) ==
		       (u16)(0xc30 + i * 0x30));
		assert(gigabyte_smartfan_tach_regs(IT87_H2RAM_SF_G3, 2, 4,
						   i + 3, &lsb, &msb));
		assert(lsb == 0xc10 + i * 2);
		assert(msb == 0xc11 + i * 2);
	}
	for (i = 0; i < 2; i++) {
		assert(gigabyte_smartfan_tach_regs(IT87_H2RAM_SF_G3, 1, 2,
						   i + 3, &lsb, &msb));
		assert(lsb == 0xc0a + i * 2);
		assert(msb == 0xc0b + i * 2);
	}
}

static void test_snapshot(enum it87_h2ram_smartfan_gen gen, u16 base)
{
	struct it87_data data = { 0 };
	struct gigabyte_smartfan_io io = {
		.data = &data,
		.read = fake_read,
		.write = fake_write,
	};
	u8 primary[5] = { 0 };
	u8 extra[9] = { 0 };
	u8 before[REG_COUNT];
	int i, n = gen == IT87_H2RAM_SF_G2 ? 3 : 9;
	int old_reads;

	for (i = 0; i < REG_COUNT; i++)
		data.regs[i] = (u8)((i * 23 + 17) & 0xff);
	memcpy(before, data.regs, sizeof(before));

	gigabyte_smartfan_save_vector(gen, &io, base, primary, extra);
	assert(data.count == 0);
	assert(data.reads == (gen == IT87_H2RAM_SF_G2 ? 7 : 14));
	for (i = 0; i < 4; i++)
		assert(primary[i] == before[base + 2 + i]);
	assert(primary[4] == (gen == IT87_H2RAM_SF_G2 ?
				  0 : before[base + 6]));
	for (i = 0; i < n; i++)
		assert(extra[i] == before[base + (gen == IT87_H2RAM_SF_G2 ?
						   6 + i * 2 : 7 + i * 4)]);

	old_reads = data.reads;
	gigabyte_smartfan_set_manual(gen, &io, base, 0xa5);
	assert(data.reads == old_reads);
	assert(data.count == n + (gen == IT87_H2RAM_SF_G2 ? 4 : 5));
	for (i = 0; i < n; i++)
		expect_write(&data, i,
			     base + (gen == IT87_H2RAM_SF_G2 ?
					     6 + i * 2 : 7 + i * 4),
			     gen == IT87_H2RAM_SF_G2 ? 0x7f : 0x64);
	expect_write(&data, n, base + 2, 0);
	expect_write(&data, n + 1, base + 3, 0x7f);
	expect_write(&data, n + 2, base + 4, 0xa5);
	expect_write(&data, n + 3, base + 5, 0);
	if (gen == IT87_H2RAM_SF_G3)
		expect_write(&data, n + 4, base + 6, 0);

	data.count = 0;
	gigabyte_smartfan_restore_vector(gen, &io, base, primary, extra);
	assert(data.count == n + (gen == IT87_H2RAM_SF_G2 ? 4 : 5));
	for (i = 0; i < n; i++)
		expect_write(&data, i,
			     base + (gen == IT87_H2RAM_SF_G2 ?
					     6 + i * 2 : 7 + i * 4), extra[i]);
	if (gen == IT87_H2RAM_SF_G3)
		expect_write(&data, n, base + 6, primary[4]);
	expect_write(&data, data.count - 4, base + 5, primary[3]);
	expect_write(&data, data.count - 3, base + 4, primary[2]);
	expect_write(&data, data.count - 2, base + 3, primary[1]);
	expect_write(&data, data.count - 1, base + 2, primary[0]);
	assert(memcmp(data.regs, before, sizeof(before)) == 0);
}

int main(void)
{
	test_layout();
	test_snapshot(IT87_H2RAM_SF_G2, 0x960);
	test_snapshot(IT87_H2RAM_SF_G3, 0x980);
	test_snapshot(IT87_H2RAM_SF_G3, 0xc30);
	puts("Gigabyte Smart Fan vector tests passed");
	return 0;
}
