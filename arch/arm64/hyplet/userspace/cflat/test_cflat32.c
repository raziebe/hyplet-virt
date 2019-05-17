#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

void hyplet_rpc_call(int a1,int a2,int a3,int a4);

/*
 * it is essential affine the program to the same 
 * core where it runs.
*/
int main(int argc, char *argv[])
{
    hyplet_rpc_call(6, 7, 9, 0x111);
}
