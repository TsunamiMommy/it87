/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef IT87_GIGABYTE_SMARTFAN_H
#define IT87_GIGABYTE_SMARTFAN_H

#include <linux/types.h>

#define GIGABYTE_SMARTFAN_ENABLE_REG	0x947

#define IT87_H2RAM_BASE_FANS		3
#define IT87_H2RAM_MAX_FANS		7
#define IT87_H2RAM_MAX_EXTRA_VECTORS	9

enum it87_h2ram_smartfan_gen {
	IT87_H2RAM_SF_NONE = 0,
	IT87_H2RAM_SF_G1,
	IT87_H2RAM_SF_G2,
	IT87_H2RAM_SF_G3,
};

struct it87_data;

struct gigabyte_smartfan_io {
	struct it87_data *data;
	int (*read)(struct it87_data *data, u16 reg);
	void (*write)(struct it87_data *data, u16 reg, u8 value);
};

int gigabyte_smartfan_channel_count(enum it87_h2ram_smartfan_gen gen,
				   u8 it57xx_fans);
u16 gigabyte_smartfan_vector_base(enum it87_h2ram_smartfan_gen gen,
				 u8 it57xx_fans, int fan);
bool gigabyte_smartfan_tach_regs(enum it87_h2ram_smartfan_gen gen,
				 u8 it57xx_version, u8 it57xx_fans,
				 int fan, u16 *lsb, u16 *msb);
void gigabyte_smartfan_save_vector(enum it87_h2ram_smartfan_gen gen,
				  const struct gigabyte_smartfan_io *io,
				  u16 base, u8 primary[5],
				  u8 extra[IT87_H2RAM_MAX_EXTRA_VECTORS]);
void gigabyte_smartfan_set_manual(enum it87_h2ram_smartfan_gen gen,
				 const struct gigabyte_smartfan_io *io,
				 u16 base, u8 pwm);
void gigabyte_smartfan_restore_vector(enum it87_h2ram_smartfan_gen gen,
				     const struct gigabyte_smartfan_io *io,
				     u16 base, const u8 primary[5],
				     const u8 extra[IT87_H2RAM_MAX_EXTRA_VECTORS]);

enum it87_h2ram_smartfan_gen
gigabyte_smartfan_detect_g23(bool allow_g3, bool allow_g2,
			     u8 major, u8 minor);
bool gigabyte_smartfan_detect_g1(bool allow_g1, u8 major, u8 minor);
u8 gigabyte_smartfan_override_mask(enum it87_h2ram_smartfan_gen gen,
				   u8 it57xx_fans, u8 mask);
void gigabyte_smartfan_set_enable(const struct gigabyte_smartfan_io *io,
				  bool enable);
void gigabyte_smartfan_claim(const struct gigabyte_smartfan_io *io,
			     enum it87_h2ram_smartfan_gen gen,
			     u8 *original, bool *saved);
void gigabyte_smartfan_release(const struct gigabyte_smartfan_io *io,
			       u8 original, bool *saved, bool release);
void gigabyte_smartfan_extvec_disable(const struct gigabyte_smartfan_io *io,
				      int fan, bool *saved, u8 snapshot[4]);
void gigabyte_smartfan_extvec_restore(const struct gigabyte_smartfan_io *io,
				      int fan, bool *saved,
				      const u8 snapshot[4], bool release);
void gigabyte_smartfan_it57xx_detect(u8 major, u8 minor, bool ecio,
				     u8 *version, u8 *count);

u16 gigabyte_smartfan_g2_base(int fan);
void gigabyte_smartfan_g2_save(const struct gigabyte_smartfan_io *io,
			      u16 base, u8 primary[5], u8 extra[9]);
void gigabyte_smartfan_g2_manual(const struct gigabyte_smartfan_io *io,
				u16 base, u8 pwm);
void gigabyte_smartfan_g2_restore(const struct gigabyte_smartfan_io *io,
				 u16 base, const u8 primary[5],
				 const u8 extra[9]);

u16 gigabyte_smartfan_g3_base(int fan);
bool gigabyte_smartfan_g3_tach(int fan, u16 *lsb, u16 *msb);
void gigabyte_smartfan_g3_save(const struct gigabyte_smartfan_io *io,
			      u16 base, u8 primary[5], u8 extra[9]);
void gigabyte_smartfan_g3_manual(const struct gigabyte_smartfan_io *io,
				u16 base, u8 pwm);
void gigabyte_smartfan_g3_restore(const struct gigabyte_smartfan_io *io,
				 u16 base, const u8 primary[5],
				 const u8 extra[9]);

u16 gigabyte_smartfan_it57xx_base(int fan);
bool gigabyte_smartfan_it57xx_tach(u8 version, u8 count,
				   int fan, u16 *lsb, u16 *msb);

#endif
