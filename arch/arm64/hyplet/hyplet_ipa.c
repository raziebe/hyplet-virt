#include <linux/module.h>
#include <linux/highmem.h>
#include <linux/hyplet.h>
#include <linux/delay.h>
#include "acqusition_trap.h"
#include "hyp_mmu.h"
#include "hypletS.h"

//
// alloc 512 * 4096  = 2MB
//
void create_level_three(struct page *pg, long *addr)
{
	int i;
	long *l3_desc;

	l3_desc = (long *) kmap(pg);
	if (l3_desc == NULL) {
		printk("%s desc NULL\n", __func__);
		return;
	}
	memset(l3_desc, 0x00, PAGE_SIZE);
	for (i = 0; i < PAGE_SIZE / sizeof(long long); i++) {
		/*
		 * Memory attribute fields in the VMSAv8-64 translation table format descriptors
		 */
		l3_desc[i] = (DESC_AF) |
				(0b11 << DESC_SHREABILITY_SHIFT) |
				/* The S2AP data access permissions, Non-secure EL1&0 translation regime  */
				(S2_PAGE_ACCESS_RW << DESC_S2AP_SHIFT) | (0b1111 << 2) |
				DESC_TABLE_BIT | DESC_VALID_BIT | (*addr);

		 (*addr) += PAGE_SIZE;
	}
	kunmap(pg);
}

// 1GB
void create_level_two(struct page *pg, long *addr)
{
	int i;
	long *l2_desc;
	struct page *pg_lvl_three;

	l2_desc = (long *) kmap(pg);
	if (l2_desc == NULL) {
		printk("%s desc NULL\n", __func__);
		return;
	}
	memset(l2_desc, 0x00, PAGE_SIZE);
	pg_lvl_three = alloc_pages(GFP_KERNEL | __GFP_ZERO, 9);
	if (pg_lvl_three == NULL) {
		printk("%s alloc page NULL\n", __func__);
		return;
	}

	for (i = 0; i < PAGE_SIZE / (sizeof(long)); i++) {
		// fill an entire 2MB of mappings
		create_level_three(pg_lvl_three + i, addr);
		// calc the entry of this table
		l2_desc[i] =
		    (page_to_phys(pg_lvl_three + i)) | DESC_TABLE_BIT |
		    DESC_VALID_BIT;
	}

	kunmap(pg);
}

void create_level_one(struct page *pg, long *addr)
{
	int i;
	long *l1_desc;
	struct page *pg_lvl_two;
	int lvl_two_nr_pages =16;

	l1_desc = (long *) kmap(pg);
	if (l1_desc == NULL) {
		printk("%s desc NULL\n", __func__);
		return;
	}
	memset(l1_desc,0x00, PAGE_SIZE);
	pg_lvl_two = alloc_pages(GFP_KERNEL | __GFP_ZERO, 4);
	if (pg_lvl_two == NULL) {
		printk("%s alloc page NULL\n", __func__);
		return;
	}

	for (i = 0; i < lvl_two_nr_pages ; i++) {
		get_page(pg_lvl_two + i);
		create_level_two(pg_lvl_two + i, addr);
		l1_desc[i] = (page_to_phys(pg_lvl_two + i)) | DESC_TABLE_BIT | DESC_VALID_BIT;

	}
	kunmap(pg);
}

void create_level_zero(struct hyplet_vm *vm, struct page *pg, long *addr)
{
	struct page *pg_lvl_one;
	long *l0_desc;

	pg_lvl_one = alloc_page(GFP_KERNEL | __GFP_ZERO);
	if (pg_lvl_one == NULL) {
		printk("%s alloc page NULL\n", __func__);
		return;
	}

	get_page(pg_lvl_one);
	create_level_one(pg_lvl_one, addr);

	l0_desc = (long *) kmap(pg);
	if (l0_desc == NULL) {
		printk("%s desc NULL\n", __func__);
		return;
	}

	memset(l0_desc, 0x00, PAGE_SIZE);
	l0_desc[0] = (page_to_phys(pg_lvl_one)) | DESC_TABLE_BIT | DESC_VALID_BIT;
	vm->pg_lvl_one = (unsigned long)pg_lvl_one;
	kunmap(pg);

}

void hyplet_init_ipa(void)
{
	long addr = 0;
	long vmid = 012;
	int starting_level = 1;
	struct hyplet_vm *vm = hyplet_get_vm();

/*
 tosz = 25 --> 39bits 64GB
	0-11
2       12-20   :512 * 4096 = 2MB per entry
1	21-29	: 512 * 2MB = per page
0	30-35 : 2^5 entries	, each points to 32 pages in level 1
 	pa range = 1 --> 36 bits 64GB

*/
	vm->pg_lvl_zero = alloc_page(GFP_KERNEL | __GFP_ZERO);
	if (vm->pg_lvl_zero == NULL) {
		printk("%s alloc page NULL\n", __func__);
		return;
	}

	get_page(vm->pg_lvl_zero);
	create_level_zero(vm, vm->pg_lvl_zero, &addr);

	if (starting_level == 0)
		vm->vttbr_el2 = page_to_phys(vm->pg_lvl_zero) | (vmid << 48);
	else
		vm->vttbr_el2 = page_to_phys((struct page *) vm->pg_lvl_one) | (vmid << 48);

	acqusion_init_procfs();
	make_vtcr_el2(vm);
}


// D-2142
void make_vtcr_el2(struct hyplet_vm *vm)
{
	long vtcr_el2_t0sz;
	long vtcr_el2_sl0;
	long vtcr_el2_irgn0;
	long vtcr_el2_orgn0;
	long vtcr_el2_sh0;
	long vtcr_el2_tg0;
	long vtcr_el2_ps;

	vtcr_el2_t0sz = hyplet_get_tcr_el1() & 0b111111;
	vtcr_el2_sl0 = 0b01;	//IMPORTANT start at level 1.  D.2143 + D4.1746
	vtcr_el2_irgn0 = 0b1;
	vtcr_el2_orgn0 = 0b1;
	vtcr_el2_sh0 = 0b11;	// inner sharable
	vtcr_el2_tg0 = (hyplet_get_tcr_el1() & 0xc000) >> 14;
	vtcr_el2_ps = (hyplet_get_tcr_el1() & 0x700000000) >> 32;

	vm->vtcr_el2 = (vtcr_el2_t0sz) |
	    (vtcr_el2_sl0 << VTCR_EL2_SL0_BIT_SHIFT) |
	    (vtcr_el2_irgn0 << VTCR_EL2_IRGN0_BIT_SHIFT) |
	    (vtcr_el2_orgn0 << VTCR_EL2_ORGN0_BIT_SHIFT) |
	    (vtcr_el2_sh0 << VTCR_EL2_SH0_BIT_SHIFT) |
	    (vtcr_el2_tg0 << VTCR_EL2_TG0_BIT_SHIFT) |
	    (vtcr_el2_ps << VTCR_EL2_PS_BIT_SHIFT);

}

/*
 * the page us using attr_ind 4
 */
void make_mair_el2(struct hyplet_vm *vm)
{
	unsigned long mair_el2;

	mair_el2 = hyplet_call_hyp(read_mair_el2);
	vm->mair_el2 = (mair_el2 & 0x000000FF00000000L ) | 0x000000FF00000000L; //
	//tvm->mair_el2 = 0xFFFFFFFFFFFFFFFFL;
 	hyplet_call_hyp(set_mair_el2, vm->mair_el2);
}

void walk_on_ipa(struct hyplet_vm *vm)
{
	int i,j,k, n;
	unsigned long *desc0 = kmap(vm->pg_lvl_zero);
	unsigned long temp;

	for ( i = 0 ; i < PAGE_SIZE/sizeof(long); i++){
		if (desc0[i]) {
			unsigned long *desc1;
			struct page *desc1_page;

			temp = desc0[i] & 0x000FFFFFFFFFFC00LL;
			printk("L0: [%d] %lx  %lx\n",i, desc0[i], temp);

			desc1_page = phys_to_page(temp);
			desc1 = kmap(desc1_page);
			for (j = 0 ; j < PAGE_SIZE/sizeof(long); j++){
				if (desc1[j]){
					unsigned long *desc2;
					struct page *desc2_page;

					temp = desc1[i] & 0x000FFFFFFFFFFC00LL;


					desc2_page = phys_to_page(temp);
					desc2 = kmap(desc2_page);

					for (k = 0 ; k < PAGE_SIZE/sizeof(long); k++){
						if (desc2[k]){
							struct page *desc3_page;
							unsigned long *desc3;

							temp = desc2[k] & 0x000FFFFFFFFFFC00LL;

							desc3_page = phys_to_page(temp);

							desc3 = kmap(desc3_page);
							for (n = 0 ; n < PAGE_SIZE/sizeof(long); n++){
								if (desc3[k]){
									temp = desc3[k] & 0x000FFFFFFFFFFC00LL;
								}
							}
							printk("L3: [%d] %lx  %lx\n",k, desc3[n], temp);
							kunmap(desc3_page);
						}
					}
					printk("L2: %lx  %lx\n", desc2[k-1], desc2[0]);
					kunmap(desc2_page);
				}
			}
			printk("L1: %lx ... %lx\n", desc1[j-1], desc1[0]);
			kunmap(desc1_page);
		}
	}
	kunmap(vm->pg_lvl_zero);
}

