/*
 * tp_mmu.h
 *
 *  Created on: Nov 9, 2017
 *      Author: raz
 */

#ifndef INCLUDE_LINUX_TP_MMU_H_
#define INCLUDE_LINUX_TP_MMU_H_

//struct _IMAGE_FILE;

//void tp_map_vmas(struct _IMAGE_FILE* image_file);
void vma_map_hyp(struct vm_area_struct* vma,pgprot_t prot);
void tp_unmmap_handler(struct task_struct* task);
void tp_mmap_handler(unsigned long addr,int len);
void tp_reset_tvm(void);
void tp_unmmap_handler(struct task_struct* task);
void map_user_space_data(void *umem,int size,pgprot_t prot);
void hyp_user_unmap(unsigned long umem,int size,int user);
int el2_do_page_fault(unsigned long addr);
int create_hyp_user_mappings(void *,void*,pgprot_t prot);

void tp_create_pg_tbl(void *cxt);
unsigned long kvm_uaddr_to_pfn(unsigned long uaddr);
int create_hyp_mappings(void *from, void *to,pgprot_t prot);
int __create_hyp_mappings(pgd_t *pgdp,
				 unsigned long start, unsigned long end,
				 unsigned long pfn, pgprot_t prot);

#define USER_TO_HYP(uva)	(uva)

#endif /* INCLUDE_LINUX_TP_MMU_H_ */
