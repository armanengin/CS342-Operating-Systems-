#include "dma.h"
#include <stdio.h>  
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <pthread.h>
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


void* p;
unsigned char* startOfSegment;
unsigned char* bitmapEnd;
int segmentSize;
int bitmapSize;
int amountOfInternalFrag;

pthread_mutex_t lockForAlloc;

int dma_init (int m){
    // allocate a cont. segment of a memory from vm using nmap -- DONE
    // allocated segment will be our heap - size 2^m 14 <= m <= 22 -- DONE
    // initialize the bitmap -- DONE  ? ? ?
    // if success 0 if not -1 -- DONE 

    // NOTLAR: address'in global olması gerekebilir diğer methodlarda nasıl erişicez buna emin olamadım çünkü

    int size = pow( 2, m); // mmap takes binary
    bitmapSize = size / pow( 2, 6); // bitmap size - depends on the size of the segment
    unsigned char* tempAddress;


    // first, checking if the segment size is in bounds
    if( size > MAX_SEG_SIZE || size < MIN_SEG_SIZE ){
        printf( "Requestes segment size out of bounds");
        return -1;
    }

    // mapping
    p = mmap( NULL, (size_t) size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); // our segment (heap) is created
    segmentSize = size; //initialize the segment size
                                                                                                // now pointer p holds the start address of the segment
    if (p == MAP_FAILED){
        printf("Mapping failed");
        return -1;
    }

    tempAddress = p; // beginning address of our heap
    startOfSegment = p; // beginning address of our heap -- hold for later
    bitmapEnd = tempAddress + bitmapSize; // ending address of our heap - bu hesaplama doğru mu emin değilim 

    // initializing bitmap
    while( tempAddress != bitmapEnd ){ // bu condition doğru mu bilmiyorum
        *tempAddress |= 0xff; // now all the bits at the address is initialized to 1
        tempAddress += 1; // increasing address by one - one word
    }
    /*
    int blockNum = (bitmapSize + 256)/16;
    if((bitmapSize + 256) % 16 != 0) blockNum++;//we need extra one more block
    int numOfWords = 2 * blockNum;

    markBitmapAllocated(0, 7, numOfWords - 1, 0);
    */

    int blockNumBitmap = bitmapSize / 16;
    if((bitmapSize) % 16 != 0) blockNumBitmap++;//we need extra one more block
    int numOfWordsBitmap = 2 * blockNumBitmap;

    int blockNumReserved = (bitmapSize + 256)/16;
    if((bitmapSize + 256) % 16 != 0) blockNumReserved++;//we need extra one more block
    int numOfWordsReserved = 2 * blockNumReserved;

    markBitmapAllocated(0, 7, numOfWordsBitmap/8 - 1, 0);//mark allocated for bitmap
    markBitmapAllocated(numOfWordsBitmap/8, 7, numOfWordsReserved/8 - 1, 0);//mark allocated for reserved
    return 0;
}

void dma_print_bitmap(){
    for( int i = 0; i < bitmapSize; i++){
        //printf("%d", *(startOfSegment + i));
        printf( BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(*(startOfSegment + i)));
        printf(" ");
        if( (i+1) % 8 == 0)
            printf("\n");
    }
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

    amountOfInternalFrag += blockNum * 16 - size;

    void * result = NULL;
    bool isFound = false; //indicates if memory is found or not

    int countOfWords = 0;//count of free words founded in the bitmap currently
    int startOfFoundI = 0;
    int startOfFoundJ = 0;
    for(int i = 0; i < bitmapSize && !isFound; ++i){
        for(int j = 7; j >= 0; --j){
            int currBit = (startOfSegment[i] >> j) & 1;
            if(currBit == 0){
                countOfWords = 0;
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
                currBit = (startOfSegment[i] >> j) & 1;
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
                        if(tempI == bitmapSize){
                            currBit = 0;
                            i = bitmapSize;
                            break;
                        }
                        int temp = (startOfSegment[tempI] >> tempJ) & 1;
                        if(temp == 1){//after finding 1, checking if there is 1 or 0 after it. If there is 0 that means we are in another used block. Keep searching unused block.
                            if(tempJ % 2 != 1){
                                break;
                            }
                        }
                    } 
                    j--;
                    if(j < 0){
                        i++;
                        j = 7;
                        if(i == bitmapSize) break;
                    }
                }
                if(i == bitmapSize) break;
                else if(currBit == 1){
                     countOfWords = 1;
                     startOfFoundI = i;
                     startOfFoundJ = j;
                }
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

void dma_print_page (int pno){
    // first check if pno is valid
    int pageNoCount = segmentSize / PAGE_SIZE;
    if( pno < 0 || pno > pageNoCount ){
        printf( "Page no is not in the interval");
        return;
    }

    unsigned char* printAddress = startOfSegment + (pno * PAGE_SIZE); // starting print address according to the page number

    for( int i = 0; i < PAGE_SIZE; i++){ // page of the SEGMENT
        printf("%02X", *(printAddress + i)); // the 0's that this prints is the REST of the program, rest of the bitmap
        if( (i + 1) % 32 == 0)
            printf("\n");
    }
    return;
}

void dma_print_blocks(){
    int isContainBitmap = 0;
    int isContainReserved = 0;
    long address;
    int sizeOfBlock;
    int i = 0;
    int j = 7;
    while(i < bitmapSize){
        int currBit = (startOfSegment[i] >> j) & 1;
        int isStart = 1;
        int startOfI = i;
        int startOfJ = j;
        if(currBit == 1){
            if(isStart){
                startOfI = i;
                startOfJ = j;
                address = (long) (startOfSegment + (64 * i + (8 - j) * 8));
                isStart = 0;
            }
            while(currBit != 0){
                j--;
                if(j < 0){
                    j = 7;
                    i++;
                    if(i + 1 == bitmapSize) break;
                }
                currBit = (startOfSegment[i] >> j) & 1;
            }
            j++;
            if(j >= 8){
                j = 0;
                i--;
            }
            sizeOfBlock = (i - startOfI) * 64 + 8 * (startOfJ - j + 1);//size of the block in bytes
            printf("F, 0x0000%lx, 0x%x, (%d)\n", address, sizeOfBlock, sizeOfBlock);
        }else{
            isContainBitmap = 0;
            isContainReserved = 0;
            if(isStart){
                startOfI = i;
                startOfJ = j;
                address = (long) (startOfSegment + (64 * i + (8 - j) * 8));
                if((64 * i + (8 - j) * 8) < bitmapSize) isContainBitmap = 1;
                if((64 * i + (8 - j) * 8) >= bitmapSize && (64 * i + (8 - j) * 8) < bitmapSize + RESERVED_SIZE) isContainReserved = 1;
                isStart = 0;
            }
            j--;
            if(j < 0){
                j = 7;
                i++;
            }
            j--;
            if(j < 0){
                j = 7;
                i++;
            }
            while(currBit != 1){
                j--;
                if(j < 0){
                    j = 7;
                    i++;
                    if(i + 1 == bitmapSize) break;
                }
                currBit = (startOfSegment[i] >> j) & 1;
            }
            if(currBit == 1){
                if(i + 1 == bitmapSize){
                    j++;
                    if(j >= 8){
                        j = 0;
                        i--;
                    }
                }
                int tempJ = j;
                int tempI = i;
                tempJ--;
                if(tempJ < 0){
                    tempJ = 7;
                    tempI++;
                } 
                int currTempBit = (startOfSegment[tempI] >> tempJ) & 1;
                if(currTempBit == 0){
                    j++;
                    if(j >= 8){
                        j = 0;
                        i--;
                    }
                }
            }
            j++;
            if(j >= 8){
                j = 0;
                i--;
            }
            sizeOfBlock = (i - startOfI) * 64 + 8 * (startOfJ - j + 1);//size of the block in bytes
            if(isContainBitmap){
                printf("A, 0x0000%lx, 0x%x, (%d), contains bitmap\n", address, sizeOfBlock, sizeOfBlock);
            }
            else if(isContainReserved){
                printf("A, 0x0000%lx, 0x%x, (%d), contains reserved\n", address, sizeOfBlock, sizeOfBlock);
            }
            else{
                printf("A, 0x0000%lx, 0x%x, (%d)\n", address, sizeOfBlock, sizeOfBlock);
            }
        }
        j--;
        if(j < 0){
            j = 7;
            i++;
        }
    }
}

int dma_give_intfrag(){
    return amountOfInternalFrag;
}

//---------------------------------------------AUXILIARY METHODS-----------------------------------------
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