/**
 * \brief  Shadow copy of asm/vdso/processor.h
 * \author Stefan Kalkowski
 * \date   2023-03-02
 */

#ifndef _LX_EMUL__SHADOW__ARCH__X86__INCLUDE__ASM__VDSO__PROCESSOR_H_
#define _LX_EMUL__SHADOW__ARCH__X86__INCLUDE__ASM__VDSO__PROCESSOR_H_

#define cpu_relax __original_linux_cpu_relax
#include_next <asm/vdso/processor.h>
#undef cpu_relax

#include <lx_emul/irq.h>
#include <lx_emul/time.h>

static __always_inline void cpu_relax(void)
{
	__original_linux_cpu_relax();

	/*
	 * When irqs are enabled, update jiffies to break potential
	 * endless busy loops like:
	 * - slchi() in drivers/i2c/algos/i2c-algo-bit.c
	 */
	if (!lx_emul_irq_state()) lx_emul_time_update_jiffies_cpu_relax();
}

#endif /* _LX_EMUL__SHADOW__ARCH__X86__INCLUDE__ASM__VDSO__PROCESSOR_H_ */
