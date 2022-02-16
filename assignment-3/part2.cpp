#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <wait.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>

#define SHM_KEY 0x1234      /* Shared memory key */
#define QUEUE_SZ 10         /* Size of bounded buffer (queue) */
#define N 1000              /* Dimension of the 2-D matrices */
#define ALL_SET -1          /* All bits set to 1 */

/* Lock */
void P(sem_t *sem)
{
    if (sem_wait(sem) < 0)
        perror("P error");
}

/* Unlock */
void V(sem_t *sem)
{
    if (sem_post(sem) < 0)
        perror("V error");
}

/* Job structure */
struct computing_job 
{
    int producer_no;    /* ID of producer/workergrp that produced this job */
    int status;         /* Rightmost 12 bits are important
                         * add(4)-copy(4)-access_progress(4)
                         */
    int matid;          /* Matrix ID */
    int mat[N][N];      /* Matrix */
    int workergrp;      /* Worker group number of the worker processing this job */
    computing_job(int pn, int s, int m, int w):producer_no(pn),status(s),matid(m),workergrp(w)
    {
        for(int i=0; i<N;i++)
        {
            for(int j=0; j<N;j++)
                mat[i][j] = -9 + rand()%19;
        }
    }
};

/* Structure for returning random unique index between 1 - 100000 */
struct randomID {
    int arrsize;
    int arr[100000];
    /* Initialise an array with arr[i] = i */
    void init(){
        arrsize = 100000;
        for(int i=0;i<100000;i++){
            arr[i]=i;
        }
    }
    /* Returns random unique index between 1 - 100000 */
    int get(){
        int idx = rand()%arrsize;
        int tmp = arr[idx];
        arr[idx] = arr[arrsize-1];
        arr[arrsize-1] = tmp;
        arrsize--;
        return tmp;
    }
};

/* Shared memory structure */
struct sharedMem      
{
    int job_counter;     /* No. of jobs added to the queue */
    int wgrp;           /* Worker group of the workers currently accessing jobs */ 
    int front;          /* Index of front of queue */
    int back;           /* Index of back of queue */
    int count;          /* Number of elements in queue currently */
    computing_job cj_queue[QUEUE_SZ]; /* Job queue of size QUEUE_SZ (bounded buffer) */
};

/* Set the access_progress bits (rightmost 4 bits) of status */
void set_block_access_status(int *status){
    int tmp = (*status & (ALL_SET << 4));
    if((*status & 0b1111) == 0b0010){
        *status = (tmp | 0b0000);
    }
    else if((*status & 0b1111) == 0b0000){
        *status = (tmp | 0b0001);
    }
    else if((*status & 0b1111) == 0b0001){
        *status = (tmp | 0b0110);
    }
    else if((*status & 0b1111) == 0b0110){
        *status = (tmp | 0b0111);
    }
    else if((*status & 0b1111) == 0b0111){
        *status = (tmp | 0b1000);
    }
    else if((*status & 0b1111) == 0b1000){
        *status = (tmp | 0b1001);
    }
    else if((*status & 0b1111) == 0b1001){
        *status = (tmp | 0b01110);
    }
    else if((*status & 0b1111) == 0b1110){
        *status = (tmp | 0b1111);
    }
}

/* Copy a block from source to dest */
void shared_to_localblock(int dest[][N/2], int source[][N], int blocknum){
    int st_row = ((blocknum >> 1) & 1) ? N/2 : 0;
    int en_row = ((blocknum >> 1) & 1) ? N-1 : N/2-1;
    int st_col = (blocknum & 1) ? N/2 : 0;
    int en_col = (blocknum & 1) ? N-1 : N/2-1;

    for(int i=st_row, x=0; i<=en_row; i++, x++){
        for(int j=st_col, y=0; j<=en_col; j++, y++){
            dest[x][y] = source[i][j];
        }
    }
}

/* Add/copy source to a block in dest specified by blocknum */
void localblock_to_shared(int dest[][N], int source[][N/2], int blocknum, int copy){
    int st_row = ((blocknum >> 1) & 1) ? N/2 : 0;
    int en_row = ((blocknum >> 1) & 1) ? N-1 : N/2-1;
    int st_col = (blocknum & 1) ? N/2 : 0;
    int en_col = (blocknum & 1) ? N-1 : N/2-1;

    for(int i=st_row, x=0; i<=en_row; i++, x++){
        for(int j=st_col, y=0; j<=en_col; j++, y++){
            if(copy)
                /* Copy source to dest */
                dest[i][j] = source[x][y];
            else
                /* Add source to dest */
                dest[i][j] += source[x][y];
        }
    }
}

/* Compute AB and store it in C */
void mat_product(int C[][N/2], int A[][N/2], int B[][N/2]){
    /* Matrix multiplication and storing in C */
    for(int i_c=0; i_c<N/2; i_c++){
        for(int j_c=0; j_c<N/2; j_c++){
            C[i_c][j_c] = 0;
        }
    }
    for(int i_c=0; i_c<N/2; i_c++){
        for(int k=0; k<N/2; k++){
            for(int j_c=0; j_c<N/2; j_c++){
                C[i_c][j_c] += A[i_c][k]*B[k][j_c];
            }
        }
    }
}

int main(int argc, char *argv[]) 
{
    time_t tbegin = time(NULL);
    srand(0);
    /* NP : No. of Producers , NW : No. of Workers , MATS : No. of matrices to be multiplied */
    int NP, NW, MATS;
    /* Take no. of producers, workers and matrices as user input */
    printf("Number of Producers: ");
    scanf("%d", &NP);
    printf("Number of Workers: ");
    scanf("%d", &NW);
    printf("Number of Matrices: ");
    scanf("%d", &MATS);
    if(NP <= 0 || NW <= 0 || MATS <= 0){
        fprintf(stderr, "Number of producers, workers and jobs must be positive (> 0)\n");
        exit(0);
    }

    /* Create a shared memory segment with the key SHM_KEY */
    int shmid;
    sharedMem* shaddr;

    shmid = shmget(SHM_KEY, sizeof(sharedMem), IPC_CREAT | 0644); 
    if(shmid < 0) {
        fprintf(stderr, "Error in allocating shared memory : %s", strerror(errno));
        exit(0);
    }
    shaddr = (sharedMem*)shmat(shmid,NULL,0);

    sem_t* sem;
    sem = sem_open("/mutex8", O_CREAT, 0644, 1); 
    
    /* Initialise the shared variables */
    shaddr->wgrp = 1;
    shaddr->front = 0;
    shaddr->back = 0;
    shaddr->job_counter = 0;
    shaddr->count = 0;

    /* Initialise a randomID variable */
    randomID rand_idx;
    rand_idx.init();

    /* Generate NP number of producers using fork() */
    for(int i=0; i<NP; i++)
    {
        pid_t pid = fork ();
        if (pid < 0) {
            /* check for error  */
            sem_unlink ("/mutex8");   
            sem_close(sem);
            /* unlink prevents the semaphore existing forever */
            /* if a crash occurs during the execution         */
            printf("Fork error.\n");
        }
        else if(pid == 0)
        {
            /* Seed rand() with PID of process */
            srand(getpid());
            /* Inside CHILD */
            while(1)
            {
                // printf("producer1 - numjobs in queue : %d , pid: %d\n", shaddr->count, getpid());
                
                /* Exit condition : All jobs have been added to the queue */
                if(shaddr->job_counter >= MATS)
                {
                    /* If MATS number of jobs have been added, stop producer */
                    // printf("1Producer w/ PID %d Exitted\n", getpid());
                    exit(0);
                }

                // printf("producer2 - numjobs in queue : %d , pid: %d\n", shaddr->count, getpid());

                /* Wait when queue is full */
                while(shaddr->count >= QUEUE_SZ - 1){
                    if(shaddr->job_counter >= MATS)
                    {
                        /* If MATS number of jobs have been added, stop producer */
                        // printf("2Producer w/ PID %d Exitted\n", getpid());
                        exit(0);
                    }
                }

                /* sleep for random time between 0 to 3 seconds */
                timespec tme = {rand()%4, 0};
                nanosleep(&tme, NULL);

                /* ---!!! Critical Section Start !!!--- */
                P(sem); /* Lock */

                // printf("producer3 - numjobs in queue : %d , pid: %d\n", shaddr->count, getpid());
                
                /* When there is space in queue and jobs are left to be produced, produce job */
                if(shaddr->count < QUEUE_SZ-1 && shaddr->job_counter < MATS){
                    int matid = 1 + rand_idx.get(); /* Random index between 1 and 100000 */
                    computing_job cj(i, 0xFF2, matid, 0); /* Initialise a computing job */

                    shaddr->count+=1;
                    shaddr->job_counter+=1;
                    shaddr->cj_queue[shaddr->back] = cj;
                    shaddr->back = (shaddr->back + 1)%QUEUE_SZ;
                    
                    printf("Produced Matrix #%d | Producer No.: %d | Producer PID: %d | Mat ID: %d\n", shaddr->job_counter, cj.producer_no, getpid(), cj.matid);
                }
                // printf("producer4 - numjobs in queue : %d , pid: %d\n", shaddr->count, getpid());
                V(sem);
                /* ---!!! Critical Section End !!!--- */
            }
            exit(0);
        }
    }

    for(int i=0; i<NW; i++)
    {
        pid_t pid = fork ();
        if (pid < 0) {
            /* check for error  */
            sem_unlink ("/mutex8");   
            sem_close(sem);  
            /* unlink prevents the semaphore existing forever */
            /* if a crash occurs during the execution         */
            printf("Fork error.\n");
        }
        else if(pid == 0)
        {
            while(1)
            {
                /* Exit condition : All jobs have been added to the queue and only one job remains */
                if(shaddr->job_counter == MATS && shaddr->count == 1)
                {
                   // printf("1Worker w/ PID %d Exitted\n", getpid());
                   exit(0);
                }
                
                // printf("worker1 - numjobs in queue : %d , pid: %d\n", shaddr->count, getpid());
                
                /* Wait if there is less than one job in the queue */
                while(shaddr->count <= 1){
                    if(shaddr->job_counter == MATS && shaddr->count == 1)
                    {
                        // printf("2Worker w/ PID %d Exitted\n", getpid());
                        exit(0);
                    }
                }
                
                // printf("worker2 - numjobs in queue : %d , pid: %d\n", shaddr->count, getpid());
                
                /* Wait if all blocks have been accessed 2 times */
                while((shaddr->cj_queue[shaddr->front].status & 0xF) == 0xF){
                    if(shaddr->job_counter == MATS && shaddr->count == 1)
                    {
                        // printf("3Worker w/ PID %d Exitted\n", getpid());
                        exit(0);
                    }
                }
                
                // printf("worker3 - numjobs in queue : %d , pid: %d  front %d\n", shaddr->count, getpid(),(shaddr->cj_queue[shaddr->front].status >> 4));

                /* Wait if the multplication computation for A or B matrix is not complete yet */
                while((((shaddr->cj_queue[shaddr->front].status >> 4) & 0xFF) != 0xFF) || (((shaddr->cj_queue[(shaddr->front+1)%QUEUE_SZ].status >> 4) & 0xFF) != 0xFF)){
                    if(shaddr->job_counter == MATS && shaddr->count == 1)
                    {
                        // printf("4Worker w/ PID %d Exitted\n", getpid());
                        exit(0);
                    }
                }

                // printf("worker4 - numjobs in queue : %d , pid: %d\n", shaddr->count, getpid());

                /* Local 2D matrices to store A{ik}, B{kj} and D{ijk} */
                int A_block[N/2][N/2], B_block[N/2][N/2], D_block[N/2][N/2];
                /* Status of A, Worker grp of worker working on A */
                int A_status, A_wgrp;
                /* Is this worker working on a job currently */
                int is_working = 0;
                /* Matrix IDs */
                int A_matid, B_matid;
                /* Producers of the Matrices */
                int A_producer, B_producer;
                
                /* sleep for random time between 0 to 3 seconds */
                timespec tme = {rand()%4, 0};
                nanosleep(&tme, NULL);

                /* ---!!! Critical Section Start !!!--- */
                P(sem);
                
                // printf("worker5 - numjobs in queue : %d , pid: %d\n", shaddr->count, getpid());
                
                /* When there are >=2 jobs in queue, there is work left and the front 2 jobs both have their values ready (mult product stored) */
                if((shaddr->count > 1) && ((shaddr->cj_queue[shaddr->front].status & 0xF) != 0xF) && (((shaddr->cj_queue[shaddr->front].status >> 4) & 0xFF) == 0xFF) && (((shaddr->cj_queue[(shaddr->front+1)%QUEUE_SZ].status >> 4) & 0xFF) == 0xFF))
                {              
                    is_working = 1;
                    // printf("worker6 - numjobs in queue : %d , pid: %d\n", shaddr->count, getpid());
                    computing_job *A = &(shaddr->cj_queue[shaddr->front]);
                    A->workergrp = shaddr->wgrp;
                    computing_job *B = &(shaddr->cj_queue[(shaddr->front+1)%QUEUE_SZ]);
                    B->workergrp = shaddr->wgrp;
                    
                    /* If this is the first worker */
                    if((A->status & 0xF) == 0b0010){
                        /* When there is space in queue and jobs are left to be produced, produce job */
                        int matid = 1 + rand_idx.get(); /* Random index between 1 and 100000 */
                        computing_job C(-shaddr->wgrp, 0x002, matid, shaddr->wgrp);
                        shaddr->cj_queue[shaddr->back] = C;
                        shaddr->back = (shaddr->back + 1)%QUEUE_SZ;
                        shaddr->count += 1;
                    }

                    /* Set access status bits appropriately */
                    set_block_access_status(&(A->status));
                    
                    /* Retrieve the front 2 matrices and store them locally for multiplication */
                    shared_to_localblock(A_block, A->mat, (A->status >> 2));
                    shared_to_localblock(B_block, B->mat, A->status);
                    A_wgrp = A->workergrp;
                    A_status = A->status;
                    A_matid = A->matid;
                    B_matid = B->matid;
                    A_producer = A->producer_no;
                    B_producer = B->producer_no;

                    printf("Read A%d & B%d | Mat IDs: %d & %d | Producer Nos.: %d & %d\n", ((A->status >> 2) & 0b0011), (A->status & 0b0011), A->matid, B->matid, A_producer, B_producer);
                    printf("No. of jobs in queue: %d | Worker PID: %d | Worker Grp: %d\n", shaddr->count, getpid(), shaddr->wgrp);
                    
                    /* If this is the last worker */
                    if((A->status & 0xF) == 0xF){
                        /* Pop the front 2 jobs */
                        shaddr->front = (shaddr->front+2)%QUEUE_SZ;
                        shaddr->count -= 2;
                        /* Next worker onwards, different worker grp */
                        shaddr->wgrp++;
                    }
                }
                
                V(sem);
                /* ---!!! Critical Section End !!!--- */

                if(is_working){
                    /* Search for the corresponding C matrix in the queue */
                    computing_job *C;
                    int flag = 0;
                    for(int i = 0; i < QUEUE_SZ; i++){
                        if((((shaddr->cj_queue[i].status >> 4) & 0xFF) != 0xFF) && (shaddr->cj_queue[i].workergrp == A_wgrp)){
                            C = &(shaddr->cj_queue[i]);
                            flag = 1;
                            break;
                        }
                    }
                    if(flag == 0){
                        printf("Corresponding C matrix not found in job queue\n");
                    }

                    /* Do multiplication of A{ik} & B{kj} and store in local D{ijk} block */
                    mat_product(D_block, A_block, B_block);

                    /* ---!!! Critical Section Start !!!--- */
                    P(sem);

                    /* Block number of C matrix in which the product is to be copied/added */
                    int blocknum = ((A_status & 0b1000) >> 2) + (A_status & 0b0001);
                    /* If copied == 1, then some other worker already copied to C, so add.
                     * Else, copy to C_blocknum
                     */
                    int copied = ((C->status >> (4 + blocknum)) & 1);

                    if(copied){
                        /* Add D{ijk} to C{ij} and update the status */
                        localblock_to_shared(C->mat, D_block, blocknum, 0);
                        C->status = (C->status | (1 << (blocknum + 8)));
                        
                        printf("Added A%d * B%d to C%d | Mat IDs: %d & %d | Producer Nos.: %d & %d\n", ((A_status >> 2) & 0b0011), (A_status & 0b0011), blocknum, A_matid, B_matid, A_producer, B_producer);
                        printf("Worker PID: %d | Worker Grp: %d\n", getpid(), A_wgrp);
                    }
                    else{
                        /* Copy D{ijk} to C{ij} and update the status */
                        localblock_to_shared(C->mat, D_block, blocknum, 1);
                        C->status = (C->status | (1 << (blocknum + 4)));
                        
                        printf("Copied A%d * B%d to C%d | Mat IDs: %d & %d | Producer Nos.: %d & %d\n", ((A_status >> 2) & 0b0011), (A_status & 0b0011), blocknum, A_matid, B_matid, A_producer, B_producer);
                        printf("Worker PID: %d | Worker Grp: %d\n", getpid(), A_wgrp);
                    }
                    
                    V(sem);
                    /* ---!!! Critical Section End !!!--- */
                    
                    // printf("after prod : %x\n", C->status >> 4);
                }
            }
            exit(0);      
        }
    }
    // printf("parent1 - count : %d\n", shaddr->count);
    
    /* Wait if all jobs have not been pushed to the queue or if only one job is not remaining */
    while(!(shaddr->job_counter == MATS && shaddr->count == 1));
    
    // printf("parent2 - count : %d\n", shaddr->count);

    /* Wait if the computation for the last remaining matrix is not complete yet */
    while(((shaddr->cj_queue[shaddr->front].status >> 4) & 0xFF) != 0xFF);

    // printf("parent3 - count : %d\n", shaddr->count);

    /* Calculate the sum of the principal diagonal of the resultant matrix */
    int trace = 0;
    for(int i=0;i<N;i++){
        trace += shaddr->cj_queue[shaddr->front].mat[i][i];
    }

    printf("Sum of elements in principal diagonal is %d\n", trace);

    /* Wait for all children to terminate */
    while(waitpid(-1, NULL, 0) > 0);
    
    /* Detach shared mem segment */
    shmdt(shaddr);
    shmctl(shmid, IPC_RMID, 0);
    
    /* Unlink and close semaphore */
    sem_unlink("/mutex8");
    sem_close(sem);

    time_t tend = time(NULL);
    printf("Program executed in %lds\n", tend - tbegin);
    printf("Parent w/ PID %d Exitted\n", getpid());
    return 0;
}
