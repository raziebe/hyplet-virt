/*
 * Copyright (C) 2012 - TrulyProtect Jayvaskula University Findland
 * Author: Raz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/errno.h>

#include <linux/arm-cci.h>

#include <linux/errno.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>

#include <linux/sched.h>
#include <linux/uaccess.h>
#include <asm/ptrace.h>
#include <asm/mman.h>
#include <asm/tlbflush.h>
#include <asm/cacheflush.h>
#include <asm/virt.h>
#include <asm/sections.h>


#include <linux/truly.h>
#include "hyp_mmu.h"

#ifdef REQUIRES_VIRT
__asm__(".arch_extension	virt");
#endif

static DEFINE_PER_CPU(unsigned long, tp_arm_hyp_stack_page);

static void __tp_cpu_init_hyp_mode(phys_addr_t pgd_ptr,
			       unsigned long hyp_stack_ptr,
			       unsigned long vector_ptr)
{
	tp_call_hyp((void*)hyp_stack_ptr, vector_ptr, pgd_ptr);
}

unsigned long get_hyp_vector(void)
{
	tp_info("Assign Truly vector\n");
	return (unsigned long)__truly_vectors;
}

static void cpu_init_hyp_mode(void)
{
	phys_addr_t pgd_ptr;
	unsigned long hyp_stack_ptr;
	unsigned long stack_page;
	unsigned long vector_ptr;

	/* Switch from the HYP stub to our own HYP init vector */
	__hyp_set_vectors(tp_get_idmap_vector());

	pgd_ptr = tp_mmu_get_httbr();
	stack_page = __this_cpu_read(tp_arm_hyp_stack_page);
	hyp_stack_ptr = stack_page + PAGE_SIZE;
	vector_ptr = get_hyp_vector();
	__tp_cpu_init_hyp_mode(pgd_ptr, hyp_stack_ptr, vector_ptr);
	tp_run_vm(NULL);
	return;
}

static int init_subsystems(void)
{
	int err = 0;

	/*
	 * Enable hardware so that subsystem initialisation can access EL2.
	 */
	cpu_init_hyp_mode();
	return err;
}

/**
 * Inits Hyp-mode on all online CPUs
 */
static int init_hyp_mode(void)
{
	int cpu;
	int err = 0;

	/*
	 * Allocate Hyp PGD and setup Hyp identity mapping
	 */
	err = tp_mmu_init();
	if (err)
		goto out_err;

	/*
	 * Allocate stack pages for Hypervisor-mode
	 */
	for_each_possible_cpu(cpu) {
		unsigned long stack_page;

		stack_page = __get_free_page(GFP_KERNEL);
		if (!stack_page) {
			err = -ENOMEM;
			goto out_err;
		}

		per_cpu(tp_arm_hyp_stack_page, cpu) = stack_page;
	}
	/*
	 * Map the Hyp-code called directly from the host
	 */
	err = create_hyp_mappings(__hyp_text_start, __hyp_text_end);
	if (err) {
		tp_err("Cannot map world-switch code\n");
		goto out_err;
	}

//	err = create_hyp_mappings(__start_rodata,
//				  __end_rodata), PAGE_HYP_RO);
//	if (err) {
//		tp_err("Cannot map rodata section\n");
//		goto out_err;
//	}

	err = create_hyp_mappings(__bss_start, __bss_stop);
	if (err) {
		tp_err("Cannot map bss section\n");
		goto out_err;
	}

	/*
	 * Map the Hyp stack pages
	 */
	for_each_possible_cpu(cpu) {
		char *stack_page = (char *)per_cpu(tp_arm_hyp_stack_page, cpu);
		err = create_hyp_mappings(stack_page, stack_page + PAGE_SIZE);
		if (err) {
			tp_err("Cannot map hyp stack\n");
			goto out_err;
		}
	}
	tp_info("Hyp mode initialized successfully\n");

	return 0;

out_err:
	tp_err("error initializing Hyp mode: %d\n", err);
	return err;
}

#define ARM_CPU_PART_CORTEX_A7		0x4100c070
#define ARM_CPU_PART_CORTEX_A15		0x4100c0f0
#define ARM_CPU_PART_MASK               0xff00fff0

static inline unsigned int __attribute_const__ read_cpuid_part(void)
{
        return read_cpuid_id() & ARM_CPU_PART_MASK;
}

int tp_target_cpu(void)
{
	switch (read_cpuid_part()) {
	case ARM_CPU_PART_CORTEX_A7:
		return TP_ARM_TARGET_CORTEX_A7;
	case ARM_CPU_PART_CORTEX_A15:
		return TP_ARM_TARGET_CORTEX_A15;
	default:
		return -EINVAL;
	}
}

static void check_tp_target_cpu(void *ret)
{
	*(int *)ret = tp_target_cpu();
}

/**
 * Initialize Hyp-mode and memory mappings on all CPUs.
 */
int tp_arch_init(void *opaque)
{
	int err;
	int ret, cpu;

	if (!is_hyp_mode_available()) {
		tp_err("HYP mode not available\n");
		return -ENODEV;
	}

	for_each_online_cpu(cpu) {
		smp_call_function_single(cpu, check_tp_target_cpu, &ret, 1);
		if (ret < 0) {
			tp_err("Error, CPU %d not supported!\n", cpu);
			return -ENODEV;
		}
	}

	err = truly_init();
	if (err)
		return err;

	err = init_hyp_mode();
	if (err)
		return -1;

	err = init_subsystems();
	if (err)
		return -1;

	return 0;
}


static int truly_boot_start(void)
{
	int rc = 0;
	
	return rc;
}

module_init(truly_boot_start);
