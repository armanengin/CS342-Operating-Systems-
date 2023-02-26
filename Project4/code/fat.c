#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdint.h>
#include <math.h>
#include <ctype.h>
#include "fat.h"

#define SECTORSIZE 512   // bytes
#define CLUSTERSIZE 1024 // bytes

char disk_name[48];
int disk_fd;

int data_start_sector = 32 + 2 * 1016;
unsigned char sector[SECTORSIZE];
unsigned char cluster[CLUSTERSIZE];
int fat_table_size;
int numOfSecPerFat;
int entryNumInFat;
struct msdos_dir_entry* foundedDirEntrySeven;
uint32_t targetClusterForEight;

int readsector (int fd, unsigned char *buf, unsigned int snum)
{
	off_t offset;
	int n;
	offset = snum * SECTORSIZE;
	lseek (fd, offset, SEEK_SET);
	n = read (fd, buf, SECTORSIZE);
	if(n == SECTORSIZE)
		return (0);
	else
		return (-1);
}

int readcluster (int fd, unsigned char *buf, unsigned int cnum)
{
	off_t offset;
	int n;
	unsigned int snum;
	snum = data_start_sector + (cnum - 2) * 2;
	offset = snum * SECTORSIZE;
	lseek(fd, offset, SEEK_SET);
	n = read(fd, buf, CLUSTERSIZE);
	if(n == CLUSTERSIZE)
		return (0);
	else	
		return (-1);
}

void print_sector(uint8_t *entry) {
    int index = 0;
    int cnt = 1;
    for (int i = 0; i < SECTORSIZE; ++i) {
        if(i == 0){
            printf("%08x: ", index);
        }
        printf("%02x ", (uint8_t) entry[i]);
        if ((i + 1) % 16 == 0){
            for(int j = (i - 16); j < (16 * cnt); j++){
                if(isprint(entry[j]) == 0){
                    printf("."); 
                }else{
                    printf("%c", entry[j]); 
                }
		    } cnt++;
            printf("\n");
            if(i+1 < SECTORSIZE) {
                index = index + 16;
                printf("%08x: ", index);
            }
        }
    }
    printf("\n");
}

void print_cluster(uint8_t *entry) {
    int index = 0;
    int cnt = 1;
    for (int i = 0; i < CLUSTERSIZE; ++i) {
        if(i == 0){
            printf("%08x: ", index);
        }
        printf("%02x ", (uint8_t) entry[i]);
        if ((i + 1) % 16 == 0) {
            for(int j = (i - 16); j < (16 * cnt); j++){
                if(isprint(entry[j]) == 0){
                    printf("."); 
                }else{
                    printf("%c", entry[j]); 
                }
			
		    } cnt++;
            printf("\n");
            if(i+1 < CLUSTERSIZE) {
                index = index + 16;
                printf("%08x: ", index);
            }
        }
    }
    printf("\n");
}

void printDirsDfs(struct msdos_dir_entry * dirEntry, int currDirNum, char* currDir){
    char* temp1 = (char*) malloc(sizeof(char)*100);
    char* temp2 = (char*) malloc(sizeof(char)*100);
    
    while(currDirNum < 32){
        if(dirEntry->name[0] == 0x00){
            dirEntry++;
            currDirNum++;
            continue;
        }
        if(dirEntry->attr != 0x08) {
            if(dirEntry->attr != 0x10){
                printf("(f) %s%s\n", currDir, dirEntry->name);
            }
            else {
                printf("(d) %s%s\n", currDir, dirEntry->name);
            }
        }
        if(dirEntry->attr == 0x10 && dirEntry->name[0] != 0xe5 && dirEntry->name[0] != 0x00 && dirEntry->name[0] != 0x2e){//enter new directory
            struct msdos_dir_entry * dirEntryForSub;
            uint32_t currClusterNum = dirEntry->starthi * pow(16, 4) + dirEntry->start;
            readcluster(disk_fd, cluster, currClusterNum);
            dirEntryForSub = (struct msdos_dir_entry *) cluster;
            temp1 = strcat(currDir ,(char*) dirEntry->name);
            temp2[0] = '/';
            temp1 = strcat(temp1 ,temp2);
            printDirsDfs(dirEntryForSub, 0, temp1);
        }
        dirEntry++;
        currDirNum++;
    }
    //free(temp1);
    //free(temp2);
}

void printPath(struct msdos_dir_entry * dirEntry, int currDirNum, char* currDir, char* target, int* isFound){
    if(*isFound) return;
    char* temp1 = (char*) malloc(sizeof(char)*100);
    char* temp2 = (char*) malloc(sizeof(char)*100);
    while(currDirNum < 32){
        if(dirEntry->attr != 0x08) {
            temp1 = strcat(currDir ,(char*) dirEntry->name);
            if(strcmp(temp1, target) == 0){
                *isFound = 1;
                int index = 0;
                *isFound = 1;
                uint32_t targetCluster = dirEntry->starthi * pow(16, 4) + dirEntry->start;
                index = targetCluster/(SECTORSIZE/4);
                readsector (disk_fd, sector, 32 + index);
                int* sec = (int ) malloc(SECTORSIZE); sec = (int) sector;
                int currClusterNum = sec[targetCluster % (SECTORSIZE/4)];
                int i = 0;
                while(currClusterNum != 0x0FFFFFF7 && currClusterNum < 0x0FFFFFF8){
                    if(i != 0){
                        index = currClusterNum/(SECTORSIZE/4);
                        readsector (disk_fd, sector, 32 + index);
                        sec = (int*) sector;
                        int currClusterNum = sec[currClusterNum % (SECTORSIZE/4)];
                    }
                    readcluster (disk_fd, cluster, currClusterNum);
                    for(int j = 0; j < CLUSTERSIZE; ++j){
                        printf("%c", cluster[j]);
                    }
                    i++;
                }
                free(sec);
            }
        }
        if(dirEntry->attr == 0x10 && dirEntry->name[0] != 0xe5 && dirEntry->name[0] != 0x00 && dirEntry->name[0] != 0x2e){//enter new directory
            struct msdos_dir_entry * dirEntryForSub;
            uint32_t currClusterNum = dirEntry->starthi * pow(16, 4) + dirEntry->start;
            readcluster(disk_fd, cluster, currClusterNum);
            dirEntryForSub = (struct msdos_dir_entry *) cluster;
            temp1 = strcat(currDir ,(char*) dirEntry->name);
            temp2[0] = '/';
            temp1 = strcat(temp1 ,temp2);
            printPath(dirEntryForSub, 0, temp1, target, isFound);
        }
        dirEntry++;
        currDirNum++;
    }
    //free(temp1);
    //free(temp2);
}

void printPathForSix(struct msdos_dir_entry * dirEntry, int currDirNum, char* currDir, char* target, int* isFound){
    if(*isFound) return;
    char* temp1 = (char*) malloc(sizeof(char)*100);
    char* temp2 = (char*) malloc(sizeof(char)*100);
    while(currDirNum < 32){
        if(dirEntry->attr != 0x08) {
            temp1 = strcat(currDir ,(char*) dirEntry->name);
            if(strcmp(temp1, target) == 0){
                *isFound = 1;
                int index = 0;
                *isFound = 1;
                uint32_t targetCluster = dirEntry->starthi * pow(16, 4) + dirEntry->start;
                index = targetCluster/(SECTORSIZE/4);
                readsector (disk_fd, sector, 32 + index);
                int* sec = (int ) malloc(SECTORSIZE); sec = (int) sector;
                int currClusterNum = sec[targetCluster % (SECTORSIZE/4)];
                int i = 0;
                int indexForPrint = 0;
                while(currClusterNum != 0x0FFFFFF7 && currClusterNum < 0x0FFFFFF8){
                    if(i != 0){
                        index = currClusterNum/(SECTORSIZE/4);
                        readsector (disk_fd, sector, 32 + index);
                        sec = (int*) sector;
                        int currClusterNum = sec[currClusterNum % (SECTORSIZE/4)];
                    }
                    readcluster (disk_fd, cluster, currClusterNum);
                    //uint8_t* tempCluster = (uint8_t*) cluster;
                    for(int j = 0; j < CLUSTERSIZE; ++j){
                        if(j == 0){
                            printf("%08x: ", indexForPrint);
                        }
                        printf("%02x ", cluster[j]);
                        if ((j + 1) % 16 == 0) {
                            printf("\n");
                            if(i+1 < CLUSTERSIZE) {
                                indexForPrint = indexForPrint + 16;
                                printf("%08x: ", indexForPrint);
                            }
                        }    
                    }
                    i++;
                }
                free(sec);
            }
        }
        if(dirEntry->attr == 0x10 && dirEntry->name[0] != 0xe5 && dirEntry->name[0] != 0x00 && dirEntry->name[0] != 0x2e){//enter new directory
            struct msdos_dir_entry * dirEntryForSub;
            uint32_t currClusterNum = dirEntry->starthi * pow(16, 4) + dirEntry->start;
            readcluster(disk_fd, cluster, currClusterNum);
            dirEntryForSub = (struct msdos_dir_entry *) cluster;
            temp1 = strcat(currDir ,(char*) dirEntry->name);
            temp2[0] = '/';
            temp1 = strcat(temp1 ,temp2);
            printPath(dirEntryForSub, 0, temp1, target, isFound);
        }
        dirEntry++;
        currDirNum++;
    }
    //free(temp1);
    //free(temp2);
}

void printPathInfo(struct msdos_dir_entry * dirEntry){
    char* temp1 = (char*) malloc(sizeof(char)*100);
    char* temp2 = (char*) malloc(sizeof(char)*100);
    int currDirNum = 0;
    while(currDirNum < 32){
        if(foundedDirEntrySeven->name[0] == 0x00){
            foundedDirEntrySeven++;
            currDirNum++;
            continue;
        } 
        if(foundedDirEntrySeven->attr != 0x08) {
            if(foundedDirEntrySeven->attr != 0x10){
                uint32_t firstClusterNum = foundedDirEntrySeven->starthi * pow(16, 4) + foundedDirEntrySeven->start;
                printf("(f) name=%-120s\t  fcn=%-10d\t  size(bytes)=%-10d\n",foundedDirEntrySeven->name,firstClusterNum,foundedDirEntrySeven->size);//add date!!!!
            }
            else {
                uint32_t firstClusterNum = foundedDirEntrySeven->starthi * pow(16, 4) + foundedDirEntrySeven->start;
                printf("(d) name=%-120s\t  fcn=%-10d\t  size(bytes)=%-10d\n",foundedDirEntrySeven->name,firstClusterNum,foundedDirEntrySeven->size);//add date!!!!
            }
        }
        foundedDirEntrySeven++;
        currDirNum++;
    }
    free(temp1);
    free(temp2);
}

void printPathForNine(struct msdos_dir_entry * dirEntry, int currDirNum, char* currDir, char* target, int* isFound){
    if(*isFound){
        return foundedDirEntrySeven;
    }
    char* temp1 = (char*) malloc(sizeof(char)*100);
    char* temp2 = (char*) malloc(sizeof(char)*100);
    while(currDirNum < 32 && *isFound != 1){
        if(dirEntry->attr != 0x08) {
            temp1 = strcat(currDir ,(char*) dirEntry->name);
            if(strcmp(temp1, target) == 0){
                *isFound = 1;
                uint32_t currClusterNum = dirEntry->starthi * pow(16, 4) + dirEntry->start;
                printf("name = %s", dirEntry->name);
                if(dirEntry->attr != 0x20){
                    printf("type = FILE", );
                }
                if(dirEntry->attr != 0x10){
                    printf("type = DIRECTORY", );
                }
                printf("firstcluster = %d", currClusterNum);
                printf("size(bytes) = %d", dirEntry->size);
            }
        }
        if(dirEntry->attr == 0x10 && dirEntry->name[0] != 0xe5 && dirEntry->name[0] != 0x00 && dirEntry->name[0] != 0x2e){//enter new directory
            struct msdos_dir_entry * dirEntryForSub;
            uint32_t currClusterNum = dirEntry->starthi * pow(16, 4) + dirEntry->start;
            readcluster(disk_fd, cluster, currClusterNum);
            dirEntryForSub = (struct msdos_dir_entry *) cluster;
            temp1 = strcat(currDir ,(char*) dirEntry->name);
            temp2[0] = '/';
            temp1 = strcat(temp1 ,temp2);
            findPath(dirEntryForSub, 0, temp1, target, isFound);
        }
        dirEntry++;
        currDirNum++;
    }
    //free(temp1);
    //free(temp2);
}

struct msdos_dir_entry * findPath(struct msdos_dir_entry * dirEntry, int currDirNum, char* currDir, char* target, int* isFound){
    if(*isFound){
        return foundedDirEntrySeven;
    }
    char* temp1 = (char*) malloc(sizeof(char)*100);
    char* temp2 = (char*) malloc(sizeof(char)*100);
    while(currDirNum < 32 && *isFound != 1){
        if(dirEntry->attr != 0x08) {
            temp1 = strcat(currDir ,(char*) dirEntry->name);
            if(strcmp(temp1, target) == 0){
                *isFound = 1;
                uint32_t currClusterNum = dirEntry->starthi * pow(16, 4) + dirEntry->start;
                readcluster(disk_fd, cluster, currClusterNum);
                foundedDirEntrySeven = (struct msdos_dir_entry *) cluster;
                return dirEntry;
            }
        }
        if(dirEntry->attr == 0x10 && dirEntry->name[0] != 0xe5 && dirEntry->name[0] != 0x00 && dirEntry->name[0] != 0x2e){//enter new directory
            struct msdos_dir_entry * dirEntryForSub;
            uint32_t currClusterNum = dirEntry->starthi * pow(16, 4) + dirEntry->start;
            readcluster(disk_fd, cluster, currClusterNum);
            dirEntryForSub = (struct msdos_dir_entry *) cluster;
            temp1 = strcat(currDir ,(char*) dirEntry->name);
            temp2[0] = '/';
            temp1 = strcat(temp1 ,temp2);
            findPath(dirEntryForSub, 0, temp1, target, isFound);
        }
        dirEntry++;
        currDirNum++;
    }
    //free(temp1);
    //free(temp2);
}

//find path method for 8th function in the document
void findPathForEight(struct msdos_dir_entry * dirEntry, int currDirNum, char* currDir, char* target, int* isFound){
    if(*isFound){
        return;
    }
    char* temp1 = (char*) malloc(sizeof(char)*100);
    char* temp2 = (char*) malloc(sizeof(char)*100);
    while(currDirNum < 32 && *isFound != 1){
        if(dirEntry->attr != 0x08) {
            temp1 = strcat(currDir ,(char*) dirEntry->name);
            if(strcmp(temp1, target) == 0){
                if(dirEntry->size == 0) targetClusterForEight = -1;
                *isFound = 1;
                targetClusterForEight = dirEntry->starthi * pow(16, 4) + dirEntry->start;
                return;
            }
        }
        if(dirEntry->attr == 0x10 && dirEntry->name[0] != 0xe5 && dirEntry->name[0] != 0x00 && dirEntry->name[0] != 0x2e){//enter new directory
            struct msdos_dir_entry * dirEntryForSub;
            uint32_t currClusterNum = dirEntry->starthi * pow(16, 4) + dirEntry->start;
            readcluster(disk_fd, cluster, currClusterNum);
            dirEntryForSub = (struct msdos_dir_entry *) cluster;
            temp1 = strcat(currDir ,(char*) dirEntry->name);
            temp2[0] = '/';
            temp1 = strcat(temp1 ,temp2);
            findPath(dirEntryForSub, 0, temp1, target, isFound);
        }
        dirEntry++;
        currDirNum++;
    }
    //free(temp1);
    //free(temp2);
}

void printPathClusterNums(){
    int index = 0;
    index = targetClusterForEight/(SECTORSIZE/4);
    readsector (disk_fd, sector, 32 + index);
    int* sec = (int ) malloc(SECTORSIZE); sec = (int) sector;
    int currClusterNum = sec[targetClusterForEight % (SECTORSIZE/4)];
    int i = 0;
    while(currClusterNum != 0x0FFFFFF7 && currClusterNum < 0x0FFFFFF8){
        if(i != 0){
            index = currClusterNum/(SECTORSIZE/4);
            readsector (disk_fd, sector, 32 + index);
            sec = (int*) sector;
            int currClusterNum = sec[currClusterNum % (SECTORSIZE/4)];
        }
        printf("cindex=%-20d\t         clustername=%-20d\n", i, currClusterNum);
        i++;
    }
    free(sec);
}

void printCountFatTable(int count){
    int entryNumPerSector = SECTORSIZE/4;
    int j = 0;
    int index = 32;
    int* sec = (int *) malloc(SECTORSIZE); 
    readsector (disk_fd, sector, index);
    sec = (int*) sector;

    for(int i = 0; i < count; ++i){
        if(j == entryNumPerSector){
            readsector (disk_fd, sector, index + 1);
            sec = (int*) sector;
            index++;
            j = 0;
        }
        if(  sec[j] < 0x0FFFFFF8){
            printf("%07d: %d\n", i,  sec[j] );
        }
        else{
            printf("%07d: EOF\n", i);
        }
        j++;
    }
}


int main(int argc, char *argv[]) {

    if (argc < 2) {
        printf("not enough arguments\n");
        exit(1);
    }

    unsigned char sector[SECTORSIZE];
    unsigned char cluster[CLUSTERSIZE];

    strcpy(disk_name, argv[1]);
    disk_fd = open(disk_name, O_SYNC | O_RDONLY);
    if( disk_fd < 0) {
        printf( "Disk image failed\n");
        exit(1);
    }

    readsector (disk_fd, sector, 0); // read sector 0
    

    struct msdos_dir_entry* dirEntry;
    struct fat_boot_sector fatBootsector;
    memcpy(&fatBootsector, sector, sizeof(struct fat_boot_sector));
    numOfSecPerFat = fatBootsector.fat_length;
    fat_table_size = numOfSecPerFat * SECTORSIZE;
    entryNumInFat = fat_table_size/4;

    foundedDirEntrySeven = (struct msdos_dir_entry*) malloc( sizeof( struct msdos_dir_entry ));

    if (strcmp(argv[2], "-v") == 0) {
        // METHOD 1 
        printf("File system type: %s\n", fatBootsector.fs_type );
    	printf("Volume label: %s\n", fatBootsector.vol_label);
    	printf("Number of sectors in disk: %u\n", fatBootsector.total_sect);
    	printf("Sector size in bytes: %d\n",SECTORSIZE );
    	printf("Number of reserved sectors: %d\n", fatBootsector.reserved );
    	printf("Number of sectors per FAT table: %u\n", fatBootsector.length);
    	printf("Number of FAT tables: %d\n",fatBootsector.fats );
    	printf("Number of sectors per cluster: %d\n", fatBootsector.sec_per_clus);
    	printf("Number of clusters = %d\n", fatBootsector.total_sect/2 ); // total sectors / 2
    	printf("Data region starts at sector: %d\n", fatBootsector.length * 2 + fatBootsector.reserved  );
    	printf("Root directory starts at sector: %d\n", fatBootsector.length * 2 + fatBootsector.reserved );
    	printf("Root directory starts at cluser: %d\n", fatBootsector.root_cluster);
    	printf("Disk size in bytes: %d", fatBootsector.total_sect * 512 );
        printf(" bytes\n");
    	printf("Disk size in Megabytes: %d", fatBootsector.total_sect * 512 / 1048576);
        printf(" MB\n");
    	printf("Number of used clusters: %d\n" );
    	printf("Number of free clusters: %d\n" );

    } else if (strcmp(argv[2], "-s") == 0) {
        // METHOD 2
        int sectorNo = atoi(argv[3]);
        readsector (disk_fd, sector, sectorNo);
        print_sector(sector);
        
    } else if (strcmp(argv[2], "-c") == 0) {
        // METHOD 3
        int clusterNo = atoi(argv[3]);
        readcluster (disk_fd, cluster, clusterNo);
        print_cluster(cluster);

    } else if (strcmp(argv[2], "-t") == 0) {
        readcluster(disk_fd, cluster, 2); // read cluster 2
        dirEntry = (struct msdos_dir_entry *) cluster;
        char* str = (char*) malloc(sizeof(char)*100); str[0] = '/';
        printDirsDfs(dirEntry, 0, str);
        free(str);
    } else if (strcmp(argv[2], "-a") == 0) {
        readcluster(disk_fd, cluster, 2); // read cluster 2
        dirEntry = (struct msdos_dir_entry *) cluster;
        char* str = (char*) malloc(sizeof(char)*100); str[0] = '/';
        int* isFound; *isFound = 0;
        printPath(dirEntry, 0, str, argv[3], isFound);
        free(str);
    } else if (strcmp(argv[2], "-b") == 0) {
        dirEntry = (struct msdos_dir_entry *) cluster;
        char* str = (char*) malloc(sizeof(char)*100); str[0] = '/';
        int* isFound; *isFound = 0;
        printPathForSix(dirEntry, 0, str, argv[3], isFound);
        free(str);
    } else if (strcmp(argv[2], "-l") == 0) {
        readcluster(disk_fd, cluster, 2); // read cluster 2
        dirEntry = (struct msdos_dir_entry *) cluster;
        char* str = (char*) malloc(sizeof(char)*100); str[0] = '/';
        int* isFound; *isFound = 0;
        printPathInfo(findPath(dirEntry, 0, str, argv[3], isFound));
    } else if (strcmp(argv[2], "-n") == 0) {
        readcluster(disk_fd, cluster, 2); // read cluster 2
        dirEntry = (struct msdos_dir_entry *) cluster;
        char* str = (char*) malloc(sizeof(char)*100); str[0] = '/';
        int* isFound; *isFound = 0;
        findPathForEight(dirEntry, 0, str, argv[3], isFound);
        if(targetClusterForEight != -1){
            printPathClusterNums();
        }
    } else if (strcmp(argv[2], "-d") == 0) {
        readcluster(disk_fd, cluster, 2); // read cluster 2
        dirEntry = (struct msdos_dir_entry *) cluster;
        char* str = (char*) malloc(sizeof(char)*100); str[0] = '/';
        int* isFound; *isFound = 0;
        printPathForNine(dirEntry, 0, str, argv[3], isFound);
    } else if (strcmp(argv[2], "-f") == 0) {
        int count = atoi(argv[3]);
        printCountFatTable(count);
    } else if (strcmp(argv[2], "-r") == 0) {
        // METHOD 11
    } else if (strcmp(argv[2], "-m") == 0) {
        // METHOD 12
    } else if (strcmp(argv[2], "-h") == 0) {
        // METHOD 13
        printf( "./fat disk1 -v\n");
        printf( "./fat disk1 -s 32\n");
        printf( "./fat disk1 -c 2\n");
        printf( "./fat disk1 -t\n");
        printf( "./fat disk1 -r /DIR2/F1.TXT 100 64\n");
        printf( "./fat disk1 -b /DIR2/F1.TXT\n");
        printf( "./fat disk1 -a /DIR2/F1.TXT\n");
        printf( "./fat disk1 -n /DIR1/AFILE.BIN\n");
        printf( "./fat disk1 -m 100\n");
        printf( "./fat disk1 -f 50\n");
        printf( "./fat disk1 -d /DIR1/AFILE.BIN\n");
        printf( "./fat disk1 -l /\n");
        printf( "./fat disk1 -l /DIR2\n");
        printf( "./fat disk1 -h\n");
    }

    return (0);
}