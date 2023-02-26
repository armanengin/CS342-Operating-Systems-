#include "dma.h"
#include <stdio.h>  
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <sys/mman.h>

#define MIN_SEG_SIZE 16384
#define MAX_SEG_SIZE 4194304
#define RESERVED_SIZE 256 // 256 bytes reserved - fixed number?
#define PAGE_SIZE 4096

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte) \
    (byte & 0x80 ? '1' : '0'), \
    (byte & 0x40 ? '1' : '0'), \
    (byte & 0x20 ? '1' : '0'), \
    (byte & 0x10 ? '1' : '0'), \
    (byte & 0x08 ? '1' : '0'), \
    (byte & 0x04 ? '1' : '0'), \
    (byte & 0x02 ? '1' : '0'), \
    (byte & 0x01 ? '1' : '0')

int size;
void* p;
unsigned char* address;
unsigned char* bitmapEnd;
unsigned char* bitmapReservedEnd;
unsigned char* bitmapManipulationAddress;
unsigned char* traverseSegment;
unsigned char* startOfSegment;
int bitmapSize;
int bitmapBSize;
int segmentSize;

pthread_mutex_t lockForAlloc;

int dma_init (int m){
    size = pow( 2, m); // mmap takes binary
    bitmapSize = size / pow( 2, 6); // bitmap size - depends on the size of the segment
    bitmapBSize = bitmapSize / pow( 2, 6);

    unsigned char* tempAddress;

    // first, checking if the segment size is in bounds
    if( size > MAX_SEG_SIZE || size < MIN_SEG_SIZE ){
        printf( "Requestes segment size out of bounds");
        return -1;
    }

    // mapping
    p = mmap( NULL, (size_t) size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); // our segment (heap) is created
                                                                                                // now pointer p holds the start address of the segment
    if (p == MAP_FAILED){
        printf("Mapping failed");
        return -1;
    }
    segmentSize = size;

    tempAddress = p; // beginning address of our heap
    address = p; // beginning address of our heap -- hold for later
    bitmapManipulationAddress = p; // beginning address of our heap -- will be used for bit manipulation in alloc
    startOfSegment = p;
    traverseSegment = p;
    bitmapEnd = tempAddress + bitmapSize; // ending address of our heap - bu hesaplama doğru mu emin değilim 
    bitmapReservedEnd = tempAddress + bitmapBSize + RESERVED_SIZE / 8;

    // initializing bitmap
    while( tempAddress <= bitmapEnd ){ // bu condition doğru mu bilmiyorum
        if( tempAddress < bitmapReservedEnd){
            if( tempAddress == p)
                *tempAddress |= 0x40;
            else
                *tempAddress |= 0x00;  
        } else {
            *tempAddress |= 0xff; // now all the bits at the address is initialized to 1
        }
        tempAddress += 1; // increasing address by one - one word
    }
    int blockNum = (bitmapSize + 256)/16;
    if((bitmapSize + 256) % 16 != 0) blockNum++;//we need extra one more block
    int numOfWords = 2 * blockNum;

    markBitmapAllocated(0, 7, numOfWords - 1, 0);

    return 0;
}

void *dma_alloc( int size ){
    // size -> requested memory size to be allocated in bytes
    // check if size is multiple of double-word-size(16)
    // return pointer to the beginning of the allocated space if not allocated return null
    
    pthread_mutex_lock(&lockForAlloc);

    if( size <= 0  || size > segmentSize - bitmapSize - RESERVED_SIZE) return NULL;

    // finding the allocated block size
    int blockNum = size/16;
    if(size % 16 != 0) blockNum++;//we need extra one more block
    int numOfWords = 2 * blockNum;

    void * result = NULL;
    bool isFound = false; //indicates if memory is found or not

    int countOfWords = 0;//count of free words founded in the bitmap currently
    int startOfFoundI = 0;
    int startOfFoundJ = 0;
    for(int i = 0; i < bitmapSize && !isFound; ++i){
        for(int j = 7; j >= 0; --j){
            int currBit = (startOfSegment[i] >> j) & 1;
            if(currBit == 0){
                if(j == 1){
                    i++;
                    j = 7;
                    if(i == bitmapSize) break;
                }
                else if(j == 0){
                    i++;
                    j = 6;
                    if(i == bitmapSize) break;
                }
                else{
                    j -= 2;//jump the 01 part
                }
                while(currBit != 1){
                    currBit = (startOfSegment[i] >> j) & 1;
                    if(currBit == 1){
                        int tempJ = j;
                        int tempI = i;
                        tempJ--;
                        if(tempJ < 0){
                            tempJ = 7;
                            tempI++;
                        }
                        if(tempI == bitmapSize) break;

                        int temp = (startOfSegment[tempI] >> tempJ) & 1;
                        if(temp == 1)//after finding 1, checking if there is 1 or 0 after it. If there is 0 that means we are in another used block. Keep searching unused block.
                            break;
                    } 
                    j--;
                    if(j < 0){
                        i++;
                        j = 7;
                        if(i == bitmapSize) break;
                    }
                }
                if(currBit == 1){
                     countOfWords = 1;
                     startOfFoundI = i;
                     startOfFoundJ = j;
                }
                else if(i == bitmapSize) break;
            }
            else{
                countOfWords++;
            }
            if(countOfWords == numOfWords){
                isFound = true;
                markBitmapAllocated(startOfFoundI, startOfFoundJ, i, j);
                result = (void*) (startOfSegment + (64 * startOfFoundI + (8 - startOfFoundJ) * 8));
            }
        }
    }

    pthread_mutex_unlock(&lockForAlloc);
    return result;
}

void markBitmapAllocated(int startOfFoundI, int startOfFoundJ, int endOfFoundI, int endOfFoundJ){
    int currIndex = startOfFoundI;
    int currJ = startOfFoundJ;
    unsigned char temp = giveByteWithZero(currJ);
    startOfSegment[currIndex] &= temp;
    currJ--;
    if(currJ < 0){
        currJ = 7;
        currIndex++;
    }
    temp = giveByteWithOne(currJ);
    startOfSegment[currIndex] |= temp;
    currJ--;
    if(currJ < 0){
        currJ = 7;
        currIndex++;
    }
    if(currIndex > endOfFoundI || currIndex == endOfFoundI && currJ < endOfFoundJ) return;
    while(currIndex <= endOfFoundI){
        temp = giveByteWithZero(currJ);
        startOfSegment[currIndex] &= temp;
        currJ--;
        if(currIndex > endOfFoundI || currIndex == endOfFoundI && currJ < endOfFoundJ) break;
        if(currJ < 0){
            currJ = 7;
            currIndex++;
        }
    }
}
unsigned char giveByteWithZero(int i){
    return 255 - pow(2, i);
}
unsigned char giveByteWithOne(int i){
    return pow(2, i);
}

void dma_free (void *p){
    unsigned long* start = (unsigned long*) startOfSegment;
    unsigned long* temp = (unsigned long*) p;
    int countOfWords = 0;
    while(temp != start){
        temp--;
        countOfWords++;
    }
    int i = countOfWords / 8;
    int j = countOfWords % 8;
    if(j == 0) i--;
    else{
        j = 8 - j;
    }
    //mark the allocated bitmap as unused
    if((startOfSegment[i] >> j) & 1 != 0) return;//if pointer is not allocated then exit the function directly
    else{
        unsigned char tempBit = giveByteWithOne(j);
        startOfSegment[i] |= tempBit;
        j--;
        if(j < 0){
            j = 7;
            i++;
        }
        tempBit = giveByteWithOne(j);
        startOfSegment[i] |= tempBit;
        j--;
        if(j < 0){
            j = 7;
            i++;
        }
        int currBit = (startOfSegment[i] >> j) & 1;
        while(currBit != 1){
            currBit = (startOfSegment[i] >> j) & 1;
            if(currBit == 1){
                int tempJ = j;
                int tempI = i;
                tempJ--;
                if(tempJ < 0){
                    tempJ = 7;
                    tempI++;
                }
                int currTempBit = (startOfSegment[tempI] >> tempJ) & 1;
                if(tempI == bitmapSize || currTempBit != 1){
                    tempJ++;
                    if(tempJ >= 8){
                        tempJ = 0;
                        tempI--;
                    }
                    tempJ++;
                    if(tempJ >= 8){
                        tempJ = 0;
                        tempI--;
                    }
                    tempBit = giveByteWithZero(tempJ);
                    startOfSegment[tempI] &= tempBit;
                }
                break;
            } 
            tempBit = giveByteWithOne(j);
            startOfSegment[i] |= tempBit;
            j--;
            if(j < 0){
                j = 7;
                i++;
            }
        }
    }
}

void dma_print_bitmap(){
    for( int i = 0; i < bitmapSize; i++){
        printf( BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(*(startOfSegment + i))); // DEĞİŞTİR'
        printf(" ");
        if( (i+1) % 8 == 0)
            printf("\n");
    }
}

void dma_print_page (int pno){
    // first chekck if pno is valid
    int pageNoCount = size / PAGE_SIZE;
    if( pno < 0 || pno > pageNoCount ){
        printf( "Page no is not in the interval");
        return;
    }

    unsigned char* printAddress = traverseSegment + (pno * PAGE_SIZE); // starting print address according to the page number

    for( int i = 0; i < PAGE_SIZE; i++){ // page of the SEGMENT
        printf("%02X", *(printAddress + i)); // the 0's that this prints is the REST of the program, rest of the bitmap
        if( (i + 1) % 32 == 0)
            printf("\n");
    }
    return;
}

void dma_print_blocks(){
    //
}

int dma_give_intfrag(){}