/*
 * offlet
 * user_hyplet.c
 *
 *  Created on: Jan 22, 2018
 *      Author: raz
 *
 *      This is an example of a user hyplet
 */

#define _GNU_SOURCE
 
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <linux/hyplet_user.h>
#include "hyplet_utils.h"


int cpu = 0;
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
	if (hyplet_map_all()) {
		fprintf(stderr, "hyplet: Failed to map a stack\n");
		return -1;
	}

	if (hyplet_set_stack((long)stack_addr, stack_size)) {
		fprintf(stderr, "hyplet: Failed to map a stack\n");
		return -1;
	}

	if (hyplet_assign_offlet(cpu, user_hyplet)) {
		fprintf(stderr, "hyplet: Failed to map code\n");
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
    cpu = atoi(argv[1]);
    printf("setting offlet to cpu %d\n", cpu);
    hyplet_start();
    printf("Waiting for offlet %d for 5 seconds\n",cpu);
    sleep(1);
    printf("some global %d\n",some_global);
}
