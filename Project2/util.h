#include <pthread.h>
#include <sys/time.h>
typedef enum {WAITING, READY, RUNNING} state;


typedef struct PCB{
    int pid;
    int nextBurstLength;
    int remainingBurstLength;
    int numOfCpuBursts;
    int numOfIoDevice1;
    int numOfIoDevice2;
    pthread_t id;
    state st;
    struct timeval startOfReadyQueue;//this value will be updated every time process enter to the ready queue
    struct timeval endOfReadyQueue;//this value will be updated every time process exit from the ready queue
    struct timeval totalTurnAroundTime;
    struct timeval startTime;//arrival time of the process
    struct timeval finishTime;//terminationtime of the process
    struct timeval totalExecutionTime;
    struct timeval totalWaitingTime;
    struct PCB* next;
    // next CPU burst length
    // total time spent in CPU ? 

}pcb; 

typedef struct CPU{
    pthread_cond_t cv;
    pthread_mutex_t lock;
}Cpu;

typedef struct IO{
    pcb* currProcess;
    pthread_cond_t cv;
    pthread_mutex_t lock;
}io;

pthread_mutex_t queue_lock;

void push(pcb** head, pcb* newNode){
    //pthread_mutex_lock(&queue_lock);
    if(*head == NULL){
        
        *head = newNode;
        newNode->next = NULL;
        //pthread_mutex_unlock(&queue_lock);
    }
    else{
        pcb* curr = *head;
        //pthread_mutex_lock(&queue_lock);
        while(curr->next != NULL) curr = curr->next;
        curr->next = newNode;
        newNode->next = NULL;
        //pthread_mutex_unlock(&queue_lock);
    }

    //pthread_mutex_unlock(&queue_lock);
}

pcb* pop(pcb** head, pcb* target){
    //pthread_mutex_lock(&queue_lock);

    if(*head == NULL) return NULL;
    if(*head == target){
        //pthread_mutex_lock(&queue_lock);
        *head = (*head)->next;
        //pthread_mutex_unlock(&queue_lock);
        return target;
    }
    else{
        pcb* curr = *head;
        pcb* prev = *head;
        while(curr != NULL){
            if(curr == target){
                //pthread_mutex_lock(&queue_lock);
                prev->next = curr->next;
                //pthread_mutex_unlock(&queue_lock);
                return target;
            }
            //pthread_mutex_lock(&queue_lock);
            prev = curr;
            curr = curr->next;
            //pthread_mutex_unlock(&queue_lock);
        }
    }
    //pthread_mutex_unlock(&queue_lock);
    return NULL;
}