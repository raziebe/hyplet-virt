
#define _GNU_SOURCE
 
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "hyplet_user.h"
#include "hyplet_utils.h"


int irq = 0;
int some_global = 0;

/*
 * This code is executed in an hyplet context
 */
long user_hyplet(void *opaque)
{
	some_global++;
}


static int hyplet_start(void)
{
	int rc;
	int cpu = -1;
	int stack_size = sysconf(_SC_PAGESIZE) * 50;
	void *stack_addr;
	/*
	 * Create a stack
	 */
	rc = posix_memalign(&stack_addr,
			    sysconf(_SC_PAGESIZE), stack_size);
	if (rc < 0) {
		fprintf(stderr, "hyplet: Failed to allocate a stack\n");
		return -1;
	}
// must fault it
	memset(stack_addr, 0x00, stack_size);
	if (hyplet_map_all(-1)) {
		fprintf(stderr, "hyplet: Failed to map a stack\n");
		return -1;
	}

	if (hyplet_set_stack(stack_addr, stack_size, cpu)) {
		fprintf(stderr, "hyplet: Failed to map a stack\n");
		return -1;
	}

	if (hyplet_set_callback(user_hyplet, cpu)) {
		fprintf(stderr, "hyplet: Failed to map code\n");
		return -1;
	}

// begin trapping
	printf("\n\n\nTrapping irq %d\n",irq);
	if (hyplet_trap_irq(irq, cpu)) {
		printf("hyplet: Failed to map user's data\n");
		return -1;
	}

	return 0;
}

/*
 * it is essential affine the program to the same 
 * core where it runs.
*/
int main(int argc, char *argv[])
{
    int rc;

    if (argc <= 1 ){
        puts("hyplet: must supply an irq , "
                        "please look in /proc/interrupts\n");
        return -1;
    }

    irq = atoi(argv[1]);
    hyplet_start();
    printf("Waiting for irq %d for 20 seconds\n", irq);
    sleep(20);
    printf("some global %d\n",some_global);
}
