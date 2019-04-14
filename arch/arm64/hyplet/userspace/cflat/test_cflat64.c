#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <dlfcn.h>

#include "../utils/hyplet_user.h"
#include "../utils/hyplet_utils.h"

int func_id = 8;

/*
 * it is essential affine the program to the same 
 * core where it runs.
*/
int main(int argc, char *argv[])
{
    
    hyplet_rpc_call(func_id,5,6,7,9);
}
