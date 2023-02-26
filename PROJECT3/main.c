#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "dma.h"

int main( int argc, char** argv){
    int ret;
    void *p1;

    ret = dma_init(14);
    if ( ret != 0 ){
        printf( "smth was wrong\n");
        exit(1);
    }
    //p1 = dma_alloc(100);
    dma_print_bitmap();

    p1 = dma_alloc(24);
    printf("\n");
    printf("---------------------------------------------------------------------------\n");
    printf("\n");
    dma_print_bitmap();
    printf("\n");
    printf("---------------------------------------------------------------------------\n");
    printf("\n");
    p1 = dma_alloc(100);
    dma_print_bitmap();
    printf("\n");
    printf("---------------------------------------------------------------------------\n");
    printf("\n");
    p1 = dma_alloc(100);
    dma_print_bitmap();
    dma_print_blocks();
}