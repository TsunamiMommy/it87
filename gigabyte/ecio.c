// SPDX-License-Identifier: GPL-2.0-or-later
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/types.h>

#include "ecio.h"

/* Defines ECIO port access for ECIO */
#define ECIO_CMD_READ   0xb0  /* Command for reading data */
#define ECIO_CMD_WRITE  0xb1  /* Command for writing data */
#define ECIO_CMD_OBF    0x01  /* Status bit mask for output buffer is full */
#define ECIO_CMD_IBF    0x02  /* Status bit mask for input buffer is full */
#define ECIO_Burst_MASK 0x10  /* Status bit mask for burst tranfers */

/* Timeouts and retries */
#define ECIO_STEP_TIMEOUT   (HZ)  /* ~1 second per wait */

static DEFINE_MUTEX(it87_ecio_lock);

/* ECIO H2RAM Access manager */

/* ------------------------------------------------------------
 * ECIO low-level I/O helpers with debug logging
 * ------------------------------------------------------------ */

static inline u8 it87_ecio_inb(u16 port)
{
	u8 value = inb(port);

	pr_debug("ECIO inb  [0x%04x] -> 0x%02x\n",
	     port, value);

	return value;
}

static inline void it87_ecio_outb(u8 value, u16 port)
{
	pr_debug("ECIO outb [0x%04x] <- 0x%02x\n",
	     port, value);

	outb(value, port);
}
/* ------------------------------------------------------------
 * Low-level wait helpers (bus busy handling)
 * ------------------------------------------------------------ */

/*
 * Wait for IBF == 0 (input buffer empty, EC ready to accept a byte).
 * Returns 0 on success, -ETIMEDOUT on timeout.
 */
static int it87_ecio_wait_ibe(void)
{
	unsigned long deadline = jiffies + ECIO_STEP_TIMEOUT;

	while(time_before(jiffies, deadline)) {
		u8 status = it87_ecio_inb(ECIO_CMD_STAT);

		/* IBF clear => we can write next byte */
		if (!(status & ECIO_CMD_IBF))
			return 0;

		cpu_relax();
		udelay(10);
	}

	return -ETIMEDOUT;
}

/*
 * Wait for OBF == 1 (output buffer full, data ready to read).
 * Returns 0 on success, -ETIMEDOUT on timeout.
 */
static int it87_ecio_wait_obf(void)
{
	unsigned long deadline = jiffies + ECIO_STEP_TIMEOUT;

	while(time_before(jiffies, deadline)) {
		u8 status = it87_ecio_inb(ECIO_CMD_STAT);

		/* OBF set => data available in ECIO_DATA */
		if (status & ECIO_CMD_OBF)
			return 0;

		cpu_relax();
		udelay(10);
	}

	return -ETIMEDOUT;
}

/* ------------------------------------------------------------
 * Single-attempt EC-IO transactions (no mutex here)
 * ------------------------------------------------------------ */

/*
 * Core read:
 *   CMD_READ -> off_hi -> off_lo -> (wait OBF) -> read data
 *
 * offset must be >= 0x0100 (high byte non-zero).
 *
 *   - WaitIBE before every write
 *   - WaitIBE after every write
 *   - WaitOBF before reading data
 */
static int it87_ecio_read_once(u16 offset, u8 *value)
{
	int err;
	u8 off_hi;
	u8 off_lo;

	if (!value)
		return -EINVAL;

	off_hi = (offset >> 8) & 0xFF;
	off_lo = offset & 0xFF;

	if (off_hi == 0)
		return -EINVAL;

	/* CMD_READ (0xB0) */
	err = it87_ecio_wait_ibe();
	if (err)
		return err;

	it87_ecio_outb(ECIO_CMD_READ, ECIO_CMD_STAT);

	err = it87_ecio_wait_ibe();
	if (err)
		return err;

	/* off_hi */
	err = it87_ecio_wait_ibe();
	if (err)
		return err;

	it87_ecio_outb(off_hi, ECIO_DATA);

	err = it87_ecio_wait_ibe();
	if (err)
		return err;

	/* off_lo */
	err = it87_ecio_wait_ibe();
	if (err)
		return err;

	it87_ecio_outb(off_lo, ECIO_DATA);

	err = it87_ecio_wait_ibe();
	if (err)
		return err;

	/* wait for data to appear */
	err = it87_ecio_wait_obf();
	if (err)
		return err;

	*value = it87_ecio_inb(ECIO_DATA);
	return 0;
}

/*
 * Core write:
 *   CMD_WRITE -> off_hi -> off_lo -> data
 *
 * offset must be >= 0x0100 (high byte non-zero).
 *
 *   - WaitIBE before every write
 *   - WaitIBE after every write (including the data byte)
 */
static int it87_ecio_write_once(u16 offset, u8 value)
{
	int err;
	u8 off_hi;
	u8 off_lo;

	off_hi = (offset >> 8) & 0xFF;
	off_lo = offset & 0xFF;

	if (off_hi == 0)
		return -EINVAL;

	/* CMD_WRITE (0xB1) */
	err = it87_ecio_wait_ibe();
	if (err)
		return err;

	it87_ecio_outb(ECIO_CMD_WRITE, ECIO_CMD_STAT);

	err = it87_ecio_wait_ibe();
	if (err)
		return err;

	/* off_hi */
	err = it87_ecio_wait_ibe();
	if (err)
		return err;

	it87_ecio_outb(off_hi, ECIO_DATA);

	err = it87_ecio_wait_ibe();
	if (err)
		return err;

	/* off_lo */
	err = it87_ecio_wait_ibe();
	if (err)
		return err;

	it87_ecio_outb(off_lo, ECIO_DATA);

	err = it87_ecio_wait_ibe();
	if (err)
		return err;

	/* data */
	err = it87_ecio_wait_ibe();
	if (err)
		return err;

	it87_ecio_outb(value, ECIO_DATA);

	err = it87_ecio_wait_ibe();
	if (err)
		return err;

	return 0;
}

/* ------------------------------------------------------------
 * Main Accessors
 * ------------------------------------------------------------ */

/*
 * gigabyte_ecio_read
 *
 *  - reg is the EC offset (0x0100..0xFFFF)
 *  - Returns 0..255 (byte) on success
 *
 */
int gigabyte_ecio_read(u16 reg)
{
	int err;
	u8 value = 0;


	mutex_lock(&it87_ecio_lock);

	err = it87_ecio_read_once(reg, &value);

	mutex_unlock(&it87_ecio_lock);

	if (err) {
		pr_debug("ECIO read failed at offset 0x%04x (err=%d)\n",
		 reg, err);
	} else {
		pr_debug("ECIO read 0x%02x from offset 0x%04x\n",
		 value, reg);
	}

	return value;
}

/*
 * gigabyte_ecio_write
 *
 *  - reg is the EC offset (0x0100..0xFFFF)
 *  - value is the byte to write
 *  - Errors are logged; no errno is returned to caller.
 *
 */
void gigabyte_ecio_write(u16 reg, u8 value)
{
	int err;


	mutex_lock(&it87_ecio_lock);

	err = it87_ecio_write_once(reg, value);

	mutex_unlock(&it87_ecio_lock);

	if (err) {
		pr_debug("ECIO write failed at offset 0x%04x "
		 "(value=0x%02x, err=%d)\n",
		 reg, value, err);
	}
}
/* End of ECIO H2RAM access manager */
