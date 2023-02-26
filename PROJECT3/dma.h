#ifndef DMA_H
#define DMA_H
//MAIN FUNCTIONS
int dma_init (int m);
void *dma_alloc (int size);
void dma_free (void *p);
void dma_print_page(int pno);
void dma_print_bitmap();
void dma_print_blocks();
int dma_give_intfrag();

//AUXILIARY FUNCTIONS
void markBitmapAllocated(int startOfFoundI, int startOfFoundJ, int endOfFoundI, int endOfFoundJ);
unsigned char giveByteWithZero(int i);
unsigned char giveByteWithOne(int i);
#endif