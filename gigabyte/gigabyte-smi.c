// SPDX-License-Identifier: GPL-2.0-or-later
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/errno.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/preempt.h>
#include <asm/irqflags.h>

#include "gigabyte.h"

#define GIGABYTE_SMI_PORT	0xb2

struct gigabyte_smi_regs {
	u64 rax;
	u64 rbx;
	u64 rcx;
	u64 rdx;
	u64 rsi;
	u64 rdi;
};

static DEFINE_MUTEX(gigabyte_smi_lock);

static int gigabyte_smi_call(struct gigabyte_smi_regs *regs)
{
	unsigned long flags;
	u64 cmd;
	u64 status;

	if (!regs)
		return -EINVAL;

	cmd = regs->rax;

	mutex_lock(&gigabyte_smi_lock);
	preempt_disable();
	local_irq_save(flags);

	asm volatile(
		"outb %%al, %%dx\n\t"
		"outb %%al, $0xeb\n\t"
		"outb %%al, $0xeb\n\t"
		: "+a"(regs->rax), "+b"(regs->rbx), "+c"(regs->rcx),
		  "+d"(regs->rdx), "+S"(regs->rsi), "+D"(regs->rdi)
		:
		: "memory"
	);

	local_irq_restore(flags);
	preempt_enable();
	status = regs->rax;
	mutex_unlock(&gigabyte_smi_lock);

	if (status != 0) {
		pr_err("Gigabyte SMI call failed, cmd=0x%04llX status=0x%016llX\n",
		       cmd & 0xffffULL, status);
		return -EIO;
	}

	return 0;
}

int gigabyte_read_id(u16 cmd, u64 *id)
{
	struct gigabyte_smi_regs regs = {
		.rax = cmd,
		.rdx = GIGABYTE_SMI_PORT,
	};
	int ret;

	if (!id)
		return -EINVAL;

	ret = gigabyte_smi_call(&regs);
	if (ret)
		return ret;

	*id = regs.rbx;
	return 0;
}
