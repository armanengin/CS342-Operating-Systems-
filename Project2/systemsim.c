#include "util.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>

// systemsim <ALG> <Q> <T1> <T2> <burst-dist> <burstlen> <min-burst>
//                                                   <max-burst> <p0> <p1> <p2> <pg> <MAXP> <ALLP> <OUTMODE>

void* generator(void* p);
void* scheduler(void* p);
void* simulator(void* p);
void ioOperation(pcb* node);
void exponentialNextBurstlengthCalculate(pcb* node);
void fixedNextBurstlengthCalculate(pcb* node);
void uniformNextBurstlengthCalculate(pcb* node);
void sjfCpuBurst(pcb* target);
void fcfsCpuBurst(pcb* target);
int rrCpuBurst(pcb* target);

pcb *readyQueue; // ready queue
int q, t1, t2, maxp, allp, outmode, burstlen, minBurst, maxBurst, outmode;
double p0, p1, p2, pg;
char alg[30];
char burst_dist[30];
pthread_t *tid;

pthread_mutex_t lockForMaxp;
pthread_mutex_t lockForCurrProcessCount;
pthread_mutex_t lockForTotalProcessCount;
pthread_mutex_t lockForReadyQueue;
pthread_mutex_t lockForScheduler;
pthread_mutex_t *lockForProcesses;

pthread_cond_t condForReadyQueue; // cond. variable for ready queue
pthread_cond_t condForScheduler;  // cond. variable for scheduler

int currProcessCount = 0;
int totalProcessCount = 0;//refers to count for number of processes created so far

io* device1;
io* device2;
Cpu* cpu;

int main(int argc, char *argv[]){
    char *ptrForStrtod = malloc(sizeof(char)*20);
    // initializing variables

    strcpy(alg, argv[1]);
    q = atoi(argv[2]);
    t1 = atoi(argv[3]);
    t2 = atoi(argv[4]);
    strcpy(burst_dist, argv[5]);
    burstlen = atoi(argv[6]);
    minBurst = atoi(argv[7]);
    maxBurst = atoi(argv[8]);
    p0 = strtod(argv[9], &ptrForStrtod);
    p1 = strtod(argv[10], &ptrForStrtod);
    p2 = strtod(argv[11], &ptrForStrtod);
    pg = strtod(argv[12], &ptrForStrtod);
    maxp = atoi(argv[13]);
    allp = atoi(argv[14]);
    outmode = atoi(argv[15]);

    tid = malloc(sizeof(pthread_t) * allp);
    lockForProcesses = malloc(sizeof(pthread_mutex_t) * allp);
    device1 = malloc(sizeof(io));
    device2 = malloc(sizeof(io));
    cpu = malloc(sizeof(Cpu));

    pthread_t generatorId;
    pthread_t schedulerId;
    pthread_mutex_init(&lockForMaxp, NULL);
    pthread_mutex_init(&lockForCurrProcessCount, NULL);
    pthread_mutex_init(&lockForTotalProcessCount, NULL);
    pthread_mutex_init(&lockForReadyQueue, NULL);
    pthread_mutex_init(&lockForScheduler, NULL);
    for(int i = 0; i < allp; ++i){
        pthread_mutex_init(&lockForProcesses[i], NULL);
    }
    pthread_cond_init(&condForReadyQueue, NULL);
    pthread_cond_init(&condForScheduler, NULL);

    pthread_mutex_init(&(cpu->lock), NULL);
    pthread_cond_init(&(cpu->cv), NULL);

    pthread_mutex_init(&(device2->lock), NULL);
    pthread_cond_init(&(device2->cv), NULL);
    pthread_mutex_init(&(device1->lock), NULL);
    pthread_cond_init(&(device1->cv), NULL);
    device1->currProcess = NULL;
    device2->currProcess = NULL;

    pthread_create(&generatorId, NULL, generator, NULL);
    pthread_create(&schedulerId, NULL, scheduler, NULL);
    pthread_join(generatorId, NULL);
    pthread_join(schedulerId, NULL);

    free(readyQueue);
    free(tid);
    free(lockForProcesses);
    free(device1);
    free(device2);
    
    printf("Program has finished\n");
}
/*
 * while not running, scheduler will sleep on its condition variable
 * checks whether or not scheduling is necessary after waking up
 *
 */
void *scheduler(void *p){
    while( totalProcessCount < allp || currProcessCount != 0){
            pthread_mutex_lock( &lockForScheduler);
        while( currProcessCount == 0){ 
            //pthread_mutex_lock( &lockForScheduler);
            pthread_cond_wait( &condForScheduler, &lockForScheduler);
            //pthread_mutex_unlock( &lockForScheduler);
        }
        int minBurst = __INT_MAX__;
        pcb *temp = readyQueue; // temp ptr for head
        int pidForSJF;
        //pthread_mutex_lock(&lockForReadyQueue);

        // algorithm check  -  rr + fcfs  -> head in ready que
        //                      sjf -> traverse the ready que and find the min
        if(readyQueue != NULL && (strcmp(alg, "FCFS") == 0 || strcmp(alg, "RR") == 0)){
            readyQueue->st = RUNNING; // head is the selected
            if(outmode == 3) printf("Process with pid: %d is selected for CPU\n", readyQueue->pid);
        } else if( strcmp(alg, "SJF") == 0){ 
            while (temp != NULL) // first traversal to find the min
            {
                if( temp->nextBurstLength < minBurst){ // finding the min cpu burst and holding its id
                    minBurst = temp->nextBurstLength;
                    pidForSJF = temp->pid;
                }
                temp = temp->next;
            }
            if(outmode == 3) printf("Process with pid: %d finished\n", pidForSJF);
            temp = readyQueue; // initializing it to head for second traversal
            while( temp != NULL ){ // second traversal to mark the process to run
                if( pidForSJF == temp->pid){ // checking pid in case there are several same length cpu's
                    temp->st = RUNNING;
                }
                temp = temp->next;
            }
        }
        pthread_cond_broadcast( &cpu->cv); // wakes all the processes up
        //pthread_mutex_unlock( &lockForReadyQueue);

        //pthread_mutex_lock( &lockForScheduler);
        pthread_cond_wait( &condForScheduler, &lockForScheduler);
        pthread_mutex_unlock( &lockForScheduler);
    }
}

void* generator(void* p){ // do not wait on any condition variable
  
    //int tempAllp = 0;
    int i = 0; // i refers to count for number of processes created so far
    if (maxp >= 10){
        for (; i < 10; ++i){
            pthread_create(&(tid[i]), NULL, &simulator, NULL);
        }
        pthread_mutex_lock(&lockForCurrProcessCount);
        currProcessCount += 10;
        pthread_cond_signal(&condForScheduler);
        pthread_mutex_unlock(&lockForCurrProcessCount);
    }
    else{
        for (; i < maxp; ++i){
            pthread_create(&(tid[i]), NULL, &simulator, NULL);
        }
        pthread_mutex_lock(&lockForCurrProcessCount);
        currProcessCount += maxp;
        pthread_mutex_unlock(&lockForCurrProcessCount);
        pthread_mutex_lock(&lockForScheduler);
        pthread_cond_signal(&condForScheduler);
        pthread_mutex_unlock(&lockForScheduler); 
    }

    while (i < allp){
        while (currProcessCount < maxp){
            if (rand() % 100 < pg * 100){// with probability pg create new process
                usleep(5000); // sleep 5ms
                pthread_create(&tid[i], NULL, simulator, NULL);
                i++;
                pthread_mutex_lock(&lockForCurrProcessCount);
                currProcessCount += 1;
                pthread_mutex_unlock(&lockForCurrProcessCount);
                if (i >= allp)
                    break;
            }
        }
    }
    for (int j = 0; j < i; ++j){
        pthread_join(tid[j], NULL);
    }
}

void *simulator(void *p){
    pcb *node = malloc(sizeof(pcb));
    //initialize pcb values for node
    node->pid = 0;
    node->nextBurstLength = 0;
    node->remainingBurstLength = 0;
    node->numOfCpuBursts = 0;
    node->numOfIoDevice1 = 0;
    node->numOfIoDevice2 = 0;
    pthread_mutex_lock(&lockForTotalProcessCount);
    totalProcessCount++;
    node->pid = totalProcessCount;
    pthread_mutex_unlock(&lockForTotalProcessCount);
    if(outmode == 3) printf("Process with pid: %d created\n", node->pid);
    pthread_mutex_lock(&lockForScheduler);
    node->st = READY;
    pthread_mutex_unlock(&lockForScheduler);
    gettimeofday(&node->startTime, NULL);

    if(strcmp(burst_dist, "fixed") == 0){
        fixedNextBurstlengthCalculate(node);
    }
    else if(strcmp(burst_dist, "exponential") == 0){//burst length calculated according to exponential distrubution
        exponentialNextBurstlengthCalculate(node);
    }
    else{//burst length calculated according to uniformal distrubition
        uniformNextBurstlengthCalculate(node);
    } 
    pthread_mutex_lock(&lockForReadyQueue);
    push(&readyQueue, node);
    gettimeofday(&node->startOfReadyQueue, NULL);

    if(outmode == 3) printf("Process with pid: %d added to the ready queue\n", node->pid); 
    pthread_mutex_unlock(&lockForReadyQueue);
    while(1){
        while(node->st != RUNNING) pthread_cond_wait(&cpu->cv, &(cpu->lock));
        
        if(strcmp(alg, "FCFS") == 0){
            pthread_mutex_lock(&(cpu->lock));
            fcfsCpuBurst(node);//process running
            pthread_mutex_unlock(&(cpu->lock));
            ioOperation(node);
        }
        else if(strcmp(alg, "SJF") == 0){
            pthread_mutex_lock(&(cpu->lock));
            sjfCpuBurst(node);//process running
            pthread_mutex_unlock(&(cpu->lock));
            ioOperation(node);
        }
        else{
            pthread_mutex_lock(&(cpu->lock));
           int isFinished = rrCpuBurst(node);//process running
           pthread_mutex_unlock(&(cpu->lock));
           if(isFinished == 1) ioOperation(node);
           else{
                node->st = READY;
                //pthread_mutex_lock(&lockForReadyQueue);
                //push(&readyQueue, node);
                
                //pthread_mutex_unlock(&lockForReadyQueue);
                pthread_mutex_lock(&lockForScheduler);
                pthread_cond_signal(&condForScheduler);
                pthread_mutex_unlock(&lockForScheduler);                      
           }

        }
    }
}

int rrCpuBurst(pcb* target){
    //pthread_mutex_lock(&lockForReadyQueue);
    pcb* nodeForScheduling;
    nodeForScheduling = pop(&readyQueue, target);
    //pthread_mutex_unlock(&lockForReadyQueue);

    if(nodeForScheduling->remainingBurstLength <= q){
        if( outmode == 2 ){
            struct timeval rr_time;
  			gettimeofday(&rr_time, NULL);
            printf( "%06ld %d RUNNING\n", rr_time.tv_usec, target->pid); 
        }
        if(outmode == 3)printf("Process with pid: %d finished\n", target->pid);
        usleep(nodeForScheduling->remainingBurstLength*1000);
        gettimeofday(&target->endOfReadyQueue, NULL);
        target->totalTurnAroundTime.tv_usec += target->endOfReadyQueue.tv_usec - target->startOfReadyQueue.tv_usec;
        target->totalExecutionTime.tv_usec += nodeForScheduling->remainingBurstLength*1000;
        target->numOfCpuBursts++;
        //pthread_mutex_lock(&lockForScheduler);
        pthread_cond_signal(&condForScheduler);
        //pthread_mutex_unlock(&lockForScheduler); //after cpu burst signal ready queue for next cpu burst
        return 1;
    }
    else{
        nodeForScheduling->remainingBurstLength -= q;
        // printing if outmode is 2
      	if( outmode == 2 ){
            struct timeval rr_time;
  			gettimeofday(&rr_time, NULL);
            printf( "%06ld %d RUNNING\n", rr_time.tv_usec, target->pid); 
        }
        usleep(q*1000);
        gettimeofday(&target->endOfReadyQueue, NULL);
        target->totalTurnAroundTime.tv_usec += target->endOfReadyQueue.tv_usec - target->startOfReadyQueue.tv_usec;
        target->totalExecutionTime.tv_usec += q*1000;
        target->numOfCpuBursts++;
        //pthread_mutex_lock(&lockForScheduler);
        pthread_cond_signal(&condForScheduler);
        //pthread_mutex_unlock(&lockForScheduler); //after cpu burst signal ready queue for next cpu burst

        //pthread_mutex_lock(&lockForReadyQueue);
        push(&readyQueue, nodeForScheduling);
        gettimeofday(&target->startOfReadyQueue, NULL);

        if(outmode == 3) printf("Process with pid: %d added to the ready queue\n", target->pid); 
        //pthread_mutex_unlock(&lockForReadyQueue);
    }
    return 0;
    
}

void fcfsCpuBurst(pcb* target){
  	// printing if outmode is 2
  	if( outmode == 2 ){
        struct timeval fcfs_time;
  		gettimeofday(&fcfs_time, NULL);
        printf( "%06ld %d RUNNING\n", fcfs_time.tv_usec, target->pid); 
    }
  
    usleep(target->nextBurstLength*1000);
    gettimeofday(&target->endOfReadyQueue, NULL);
    target->totalTurnAroundTime.tv_usec += target->endOfReadyQueue.tv_usec - target->startOfReadyQueue.tv_usec;
    target->totalExecutionTime.tv_usec += target->nextBurstLength*1000;
    target->numOfCpuBursts++;
    if(outmode == 3)printf("Process with pid: %d finished\n", target->pid);
    //pthread_mutex_lock(&lockForScheduler);
    pthread_cond_signal(&condForScheduler);
    //pthread_mutex_unlock(&lockForScheduler); //after cpu burst signal ready queue for next cpu burst

    //pthread_mutex_lock(&lockForReadyQueue);
    pop(&readyQueue, target);
    //pthread_mutex_unlock(&lockForReadyQueue);
}

void sjfCpuBurst(pcb* target){
    // printing if outmode is 2
  	if( outmode == 2 ){
        struct timeval sjf_time;
  		gettimeofday(&sjf_time, NULL);
          target->totalTurnAroundTime.tv_usec += target->endOfReadyQueue.tv_usec - target->startOfReadyQueue.tv_usec;
        printf( "%06ld %d RUNNING\n", sjf_time.tv_usec, target->pid); 
    }
    
    usleep(target->nextBurstLength*1000);
    gettimeofday(&target->endOfReadyQueue, NULL);
    target->totalExecutionTime.tv_usec += target->nextBurstLength*1000;
    target->numOfCpuBursts++;
    if(outmode == 3)printf("Process with pid: %d finished\n", target->pid);
    //pthread_mutex_lock(&lockForScheduler);
    pthread_cond_signal(&condForScheduler);
    //pthread_mutex_unlock(&lockForScheduler); //after cpu burst signal ready queue for next cpu burst

    //pthread_mutex_lock(&lockForReadyQueue);
    pop(&readyQueue, target);
    //pthread_mutex_unlock(&lockForReadyQueue);
}

void fixedNextBurstlengthCalculate(pcb* node){
    node->nextBurstLength = burstlen;
    node->remainingBurstLength = burstlen;
}

void exponentialNextBurstlengthCalculate(pcb* node){
    int expBurst = 0;
    double u = (double)rand() / (double)RAND_MAX;
    do{
        expBurst = (-1* log(u))*burstlen;
    }while(minBurst <= expBurst && maxBurst >= expBurst);
    node->nextBurstLength = expBurst;
    node->remainingBurstLength = expBurst;
}

void uniformNextBurstlengthCalculate(pcb* node){
    node->nextBurstLength = minBurst + rand() % (maxBurst + 1 - minBurst);
    node->remainingBurstLength = node->nextBurstLength;    
}

void ioOperation(pcb* node){
    int value = rand();
    if(value % 100 < p0 * 100){//terminate and print information about terminated process
        pthread_mutex_lock(&lockForCurrProcessCount);
        currProcessCount--;
        pthread_mutex_unlock(&lockForCurrProcessCount);
        gettimeofday(&node->finishTime, NULL);
        node->totalWaitingTime.tv_usec = node->totalTurnAroundTime.tv_usec - node->totalExecutionTime.tv_usec;
        printf("process with pid = %d terminated with following datas => pid = %d, arv = %06ld, dept = %06ld, cpu = %06ld, waitr = %06ld, turna = %06ld, n-bursts = %d, n-d1 = %d, n-d2 = %d\n",node->pid, node->pid, node->startTime.tv_usec, node->finishTime.tv_usec, node->totalExecutionTime.tv_usec, node->totalWaitingTime.tv_usec, node->totalTurnAroundTime.tv_usec, node->numOfCpuBursts, node->numOfIoDevice1, node->numOfIoDevice2);
        pthread_exit(NULL);
    }
    else if(value % 100 < p1 + 100){//go to device 1
        if(device1->currProcess == NULL){//no device running on the device1
            pthread_mutex_lock(&device1->lock);
            device1->currProcess = node;
            node->st = WAITING;
                    
            if( outmode == 2 ){
                struct timeval io1_time;
  				gettimeofday(&io1_time, NULL);
                printf( "%06ld %d USING DEVICE 1\n", io1_time.tv_usec, node->pid ); // ex: 4210 9 USING DEVICE1
            }
            else if(outmode == 3){
                printf("Process with pid: %d doing I/O with device1\n", node->pid);
            }
            pthread_mutex_unlock(&device1->lock);  
            usleep(t1*1000);
            node->numOfIoDevice1++;

            pthread_mutex_lock(&device1->lock);
            device1->currProcess = NULL;
            //calculate next burst length before pushing ready queue
            if(strcmp(burst_dist, "fixed") == 0){
                fixedNextBurstlengthCalculate(node);
            }
            else if(strcmp(burst_dist, "exponential") == 0){//burst length calculated according to exponential distrubution
                exponentialNextBurstlengthCalculate(node);
            }
            else{//burst length calculated according to uniformal distrubition
                uniformNextBurstlengthCalculate(node);
            }
            node->st = READY;
            //pthread_mutex_lock(&lockForReadyQueue);
            push(&readyQueue, node);
            gettimeofday(&node->startOfReadyQueue, NULL);

            if(outmode == 3) printf("Process with pid: %d added to the ready queue\n", node->pid); 
            //pthread_mutex_unlock(&lockForReadyQueue);

            //pthread_mutex_lock(&lockForScheduler);
            pthread_cond_signal(&condForScheduler);
            //pthread_mutex_unlock(&lockForScheduler);                      
            pthread_cond_signal(&device1->cv);
            pthread_mutex_unlock(&device1->lock);
        }
        else{//while there are other processes waiting on device1
            pthread_mutex_lock(&device1->lock);
            while(device1->currProcess != NULL) pthread_cond_wait(&device1->cv, &device1->lock);

            device1->currProcess = node;
            node->st = WAITING;
                    
            if( outmode == 2 ){
                struct timeval io1_time;
  				gettimeofday(&io1_time, NULL);
                printf( "%06ld %d USING DEVICE 1\n", io1_time.tv_usec, node->pid ); // ex: 4210 9 USING DEVICE1
            }
            else if(outmode == 3){
                printf("Process with pid: %d doing I/O with device1\n", node->pid);
            }
            pthread_mutex_unlock(&device1->lock);  
            usleep(t1*1000);
            node->numOfIoDevice1++;

            pthread_mutex_lock(&device1->lock);
            device1->currProcess = NULL;
            //calculate next burst length before pushing ready queue
            if(strcmp(burst_dist, "fixed") == 0){
                fixedNextBurstlengthCalculate(node);
            }
            else if(strcmp(burst_dist, "exponential")){//burst length calculated according to exponential distrubution
                exponentialNextBurstlengthCalculate(node);
            }
            else{//burst length calculated according to uniformal distrubition
                uniformNextBurstlengthCalculate(node);
            }
            node->st = READY;
            //pthread_mutex_lock(&lockForReadyQueue);
            push(&readyQueue, node);
            gettimeofday(&node->startOfReadyQueue, NULL);

            if(outmode == 3) printf("Process with pid: %d added to the ready queue\n", node->pid); 
            //pthread_mutex_unlock(&lockForReadyQueue);

            //pthread_mutex_lock(&lockForScheduler);
            pthread_cond_signal(&condForScheduler);
            //pthread_mutex_unlock(&lockForScheduler);                      
            pthread_cond_signal(&device1->cv);
            pthread_mutex_unlock(&device1->lock);
        }
    }
    else{//go to device 2
        // DEVICE 2 CODE
        if(device2->currProcess == NULL){//no device running on the device2
            pthread_mutex_lock(&device2->lock);
            device2->currProcess = node;
            node->st = WAITING;
                    
            if( outmode == 2 ){
                struct timeval io1_time;
  				gettimeofday(&io1_time, NULL);
                printf( "%06ld %d USING DEVICE 1\n", io1_time.tv_usec, node->pid ); // ex: 4210 9 USING DEVICE2
            }
            else if(outmode == 3){
                printf("Process with pid: %d doing I/O with device2\n", node->pid);    
            }
            pthread_mutex_unlock(&device2->lock);  
            usleep(t1*1000);//sleep
            node->numOfIoDevice2++;

            pthread_mutex_lock(&device2->lock);
            device2->currProcess = NULL;
            //calculate next burst length before pushing ready queue
            if(strcmp(burst_dist, "fixed") == 0){
                fixedNextBurstlengthCalculate(node);
            }
            else if(strcmp(burst_dist, "exponential") == 0){//burst length calculated according to exponential distrubution
                exponentialNextBurstlengthCalculate(node);
            }
            else{//burst length calculated according to uniformal distrubition
                uniformNextBurstlengthCalculate(node);
            }
            node->st = READY;
            //pthread_mutex_lock(&lockForReadyQueue);
            push(&readyQueue, node);
            gettimeofday(&node->startOfReadyQueue, NULL);
            
            if(outmode == 3) printf("Process with pid: %d added to the ready queue\n", node->pid); 
            //pthread_mutex_unlock(&lockForReadyQueue);

            //pthread_mutex_lock(&lockForScheduler);
            pthread_cond_signal(&condForScheduler);
            //pthread_mutex_unlock(&lockForScheduler);                      
            pthread_cond_signal(&device2->cv);
            pthread_mutex_unlock(&device2->lock);
        }
        else{//while there are other processes waiting on device2
            pthread_mutex_lock(&device2->lock);
            while(device2->currProcess != NULL) pthread_cond_wait(&device2->cv, &device2->lock);

            device2->currProcess = node;
            node->st = WAITING;
                    
            if( outmode == 2 ){
                struct timeval io1_time;
  				gettimeofday(&io1_time, NULL);
                printf( "%06ld %d USING DEVICE 2\n", io1_time.tv_usec, node->pid ); // ex: 4210 9 USING DEVICE2
            }
            else if(outmode == 3){
                printf("Process with pid: %d doing I/O with device2\n", node->pid);        
            }
            pthread_mutex_unlock(&device2->lock);  
            usleep(t1*1000);//sleep
            node->numOfIoDevice2++; 

            pthread_mutex_lock(&device2->lock);
            device2->currProcess = NULL;
            //calculate next burst length before pushing ready queue
            if(strcmp(burst_dist, "fixed") == 0){
                fixedNextBurstlengthCalculate(node);
            }
            else if(strcmp(burst_dist, "exponential") == 0){//burst length calculated according to exponential distrubution
                exponentialNextBurstlengthCalculate(node);
            }
            else{//burst length calculated according to uniformal distrubition
                uniformNextBurstlengthCalculate(node);
            }
            node->st = READY;
            //pthread_mutex_lock(&lockForReadyQueue);
            push(&readyQueue, node);
            gettimeofday(&node->startOfReadyQueue, NULL);

            if(outmode == 3) printf("Process with pid: %d added to the ready queue\n", node->pid); 
            //pthread_mutex_unlock(&lockForReadyQueue);

            //pthread_mutex_lock(&lockForScheduler);
            pthread_cond_signal(&condForScheduler);
            //pthread_mutex_unlock(&lockForScheduler);                      
            pthread_cond_signal(&device2->cv);
            pthread_mutex_unlock(&device2->lock);
        }            	
    }    
}