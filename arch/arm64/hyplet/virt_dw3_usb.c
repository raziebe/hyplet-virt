#include <linux/module.h>
#include <linux/highmem.h>
#include <linux/hyplet.h>
#include <linux/delay.h>

#include "hyp_mmu.h"
#include "malware_trap.h"



void prepare_special_addresses(struct hyplet_vm *vm)
{
	int i;
	struct hyplet_driver_handler* hyphnd;

	for (i = 0 ;i < FAULT_MAX_HANDLERS; i++){
		hyphnd = &vm->dev_access->hyphnd[i];
		hyphnd->offset = -1;
		hyphnd->action = NULL;
	}
	/*
	 * Now prepare the real actions
	 */
	hyphnd = &vm->dev_access->hyphnd[0];
	hyphnd->offset = 0xb00;
	hyphnd->action = (__action__mmio__)KERN_TO_HYP(dwc3_mmio_abrt_action10);
}


void malware_prep_mmio(char *addr)
{
	/* A marker */
	memset(addr, 0xeeeeeeee, PAGE_SIZE);
	mb();
}

/*
 * An example for how the user can access an modify
 * MMIO mapped page.
 */
void __hyp_text dwc3_mmio_abrt_action10(struct hyplet_vm *vm, struct hyplet_driver_handler *hyphnd)
{
	struct virt_dev_access* virtdev = (struct virt_dev_access*)KERN_TO_HYP(vm->dev_access);
	unsigned long el2_mmio_addr = KERN_TO_HYP(virtdev->faddr.fake_vaddr);
	unsigned long *p;

	p = (unsigned long *)(el2_mmio_addr + hyphnd->offset);

	if ( (*p & 0x1) )
		(	*p) &= ~(0x1);
		else
			(*p) |= 0x1;

}
