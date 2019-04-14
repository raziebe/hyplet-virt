#include <linux/module.h>
#include <linux/highmem.h>
#include <linux/hyplet.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>		/* for file_operations */
#include <linux/slab.h>	/* versioning */
#include <linux/cdev.h>
#include "acqusition_trap.h"
#include "hyp_mmu.h"

/*
 * Called in EL2 to handle a faulted address
 */
int __hyp_text hyplet_handle_abrt(struct hyplet_vm *vm, unsigned long addr)
{
//	struct hyplet_driver_handler* hyphnd;
	return 1;
}


static long make_special_page_desc(unsigned long real_phyaddr,int s2_rw)
{
	unsigned long addr = real_phyaddr;

	return (DESC_AF) | (0b11 << DESC_SHREABILITY_SHIFT) |
	                ( s2_rw  << DESC_S2AP_SHIFT) | (0b1111 << 2) |
	                  DESC_TABLE_BIT | DESC_VALID_BIT | addr;
}

/* user interface  */
static struct proc_dir_entry *procfs = NULL;

static ssize_t proc_write(struct file *file, const char __user * buffer,
			  size_t count, loff_t * dummy)
{
	struct hyplet_vm *vm;

	vm = hyplet_get_vm();
	/*
	 * mark all pages RO
	*/
	//make_special_page_desc(phys_addr, S2_PAGE_ACCESS_R);
	walk_on_ipa(vm);
	return count;
}

static int proc_open(struct inode *inode, struct file *filp)
{
	filp->private_data = (void *)0x01;
	return 0;
}

static ssize_t proc_read(struct file *filp, char __user * page,
			 size_t size, loff_t * off)
{
	ssize_t len = 0;
	int cpu;
	struct hyplet_vm *vm = hyplet_get_vm();

	if ( filp->private_data == 0)
		return 0;

	for_each_possible_cpu(cpu){
		vm = hyplet_get(cpu);
	//	len += sprintf(page + len,
		//		"%d LastCurrent\n");
	}

	filp->private_data = 0x00;
	return len;
}


static struct file_operations acqusition_proc_ops = {
	.open = proc_open,
	.read = proc_read,
	.write = proc_write,
};


static ssize_t acqusition_ops_write(struct file *filp,
	const char __user *umem, size_t size, loff_t *off)
{
	int n = 0;
	struct hyplet_vm *vm =  hyplet_get_vm();

	return size-n;
}

static ssize_t acqusition_ops_read(struct file *filp, char __user *umem,
				size_t size, loff_t *off)
{
	int n = 0;
	struct hyplet_vm *vm =  hyplet_get_vm();
	return size-n;
}

static struct file_operations acqusition_ops = {
	write: acqusition_ops_write,
	read:  acqusition_ops_read,
};

int acqusition_ops_major = 0;


void acqusion_init_procfs(void)
{
	procfs = proc_create_data("hyplet_stats", 
			O_RDWR, NULL, &acqusition_proc_ops, NULL);
	
	acqusition_ops_major = register_chrdev(0, "acqusition", &acqusition_ops);
	if (acqusition_ops_major < 0){
		printk(MODULE_NAME "Failed to create acqusition procfs");
	}
}


