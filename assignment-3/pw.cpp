#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <wait.h>
#include <pthread.h>
#include <sys/shm.h>
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
    int jobCounter;     /* No. of jobs added to the queue */
    int wgrp;           /* Worker group of the workers currently accessing jobs */ 
    int front;          /* Index of front of queue */
    int back;           /* Index of back of queue */
    int count;          /* Number of elements in queue currently */
    computing_job cjQueue[QUEUE_SZ]; /* Job queue of size QUEUE_SZ (bounded buffer) */
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

void copy_block(int dest[][N/2], int source[][N], int blocknum){
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

/* Compute AB and add/copy it to C_blocknum */
void mat_product(int C[][N], int copied, int blocknum, int A[][N/2], int B[][N/2]){
    /* Calculation of starting/ending row/column based on blocknum */
    int st_row = ((blocknum >> 1) & 1) ? N/2 : 0;
    int en_row = ((blocknum >> 1) & 1) ? N-1 : N/2-1;
    int st_col = (blocknum & 1) ? N/2 : 0;
    int en_col = (blocknum & 1) ? N-1 : N/2-1;

    /* Matrix multiplication and storing in appropriate block of C */
    for(int i_c=st_row, i_a=0; i_c<=en_row; i_c++, i_a++){
        for(int j_c=st_col, i_b=0; j_c<=en_col; j_c++, i_b++){
            int res = 0;
            for(int x=0; x<N/2; x++){
                res += A[i_a][x]*B[x][i_b];
            }
            if(copied){
                C[i_c][j_c] += res;
            }
            else{
                C[i_c][j_c] = res;
            }
        }
    }
}

int main(int argc, char *argv[]) 
{
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
    sem = sem_open("/mutex7", O_CREAT, 0644, 1); 
    
    /* Initialise the shared variables */
    shaddr->wgrp = 1;
    shaddr->front = 0;
    shaddr->back = 0;
    shaddr->jobCounter = 0;
    shaddr->count = 0;

    /* Initialise a randomID variable */
    randomID rand_idx;
    rand_idx.init();

    for(int i=0; i<NP; i++)
    {
        pid_t pid = fork ();
        if (pid < 0) {
            /* check for error  */
            sem_unlink ("/mutex7");   
            sem_close(sem);
            /* unlink prevents the semaphore existing forever */
            /* if a crash occurs during the execution         */
            printf("Fork error.\n");
        }
        else if(pid == 0)
        {
            while(1)
            {
                // printf("producer1 - numjobs in queue : %d , pid: %d\n", shaddr->count, getpid());
                if(shaddr->jobCounter >= MATS)
                {
                    /* If MATS number of jobs have been added, stop producer */
                    printf("Producer w/ PID %d Exitted\n", getpid());
                    exit(0);
                }

                // printf("producer2 - numjobs in queue : %d , pid: %d\n", shaddr->count, getpid());

                /* Wait when queue is full */
                while(shaddr->count >= QUEUE_SZ - 1){
                    if(shaddr->jobCounter >= MATS)
                    {
                        /* If MATS number of jobs have been added, stop producer */
                        printf("Producer w/ PID %d Exitted\n", getpid());
                        exit(0);
                    }
                }

                /* ---!!! Critical Section Start !!!--- */
                P(sem);
                // printf("producer3 - numjobs in queue : %d , pid: %d\n", shaddr->count, getpid());
                if(shaddr->count < QUEUE_SZ-1 && shaddr->jobCounter < MATS){
                    /* When there is space in queue and jobs are left to be produced, produce job */
                    int matid = 1 + rand_idx.get(); /* Random index between 1 and 100000 */
                    computing_job cj(i, 0xFF2, matid, 0);
                    
                    sleep(rand()%4);

                    shaddr->jobCounter+=1;
                    shaddr->cjQueue[shaddr->back] = cj;
                    shaddr->back = (shaddr->back + 1)%QUEUE_SZ;
                    shaddr->count+=1;
                    // for(int i=0;i<N;i++){
                    //     for(int j=0;j<N;j++){
                    //         printf("%d ", cj.mat[i][j]);
                    //     }
                    //     printf("\n");
                    // }
                    printf("Computing Job Added\nProducer No: %d, Pid: %d, Matrix ID: %d", cj.producer_no, getpid(), cj.matid);
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
            sem_unlink ("/mutex7");   
            sem_close(sem);  
            /* unlink prevents the semaphore existing forever */
            /* if a crash occurs during the execution         */
            printf("Fork error.\n");
        }
        else if(pid == 0)
        {
            while(1)
            {
                if(shaddr->jobCounter == MATS && shaddr->count == 1)
                {
                   printf("Worker w/ PID %d Exitted\n", getpid());
                   exit(0);
                }
                sleep(rand()%4);
                
                // printf("worker1 - numjobs in queue : %d , pid: %d\n", shaddr->count, getpid());
                
                /* Wait if there is only one job in the queue */
                while(shaddr->count == 1){
                    if(shaddr->jobCounter == MATS && shaddr->count == 1)
                    {
                        printf("Worker w/ PID %d Exitted\n", getpid());
                        exit(0);
                    }
                }
                
                // printf("worker2 - numjobs in queue : %d , pid: %d\n", shaddr->count, getpid());
                
                /* Wait if all blocks have been accessed 2 times */
                while((shaddr->cjQueue[shaddr->front].status & 0xF) == 0xF){
                    if(shaddr->jobCounter == MATS && shaddr->count == 1)
                    {
                        printf("Worker w/ PID %d Exitted\n", getpid());
                        exit(0);
                    }
                }
                
                // printf("worker3 - numjobs in queue : %d , pid: %d  front %d\n", shaddr->count, getpid(),(shaddr->cjQueue[shaddr->front].status >> 4));

                /* Wait if the multplication computation for A or B matrix is not complete yet */
                while(((shaddr->cjQueue[shaddr->front].status >> 4) & 0xFF) != 0xFF || ((shaddr->cjQueue[(shaddr->front+1)%QUEUE_SZ].status >> 4) & 0xFF) != 0xFF){
                    if(shaddr->jobCounter == MATS && shaddr->count == 1)
                    {
                        printf("Worker w/ PID %d Exitted\n", getpid());
                        exit(0);
                    }
                }

                // printf("worker4 - numjobs in queue : %d , pid: %d\n", shaddr->count, getpid());

                int A_block[N/2][N/2], B_block[N/2][N/2];
                int A_status, A_wgrp;
                int is_working = 0;
                
                /* ---!!! Critical Section Start !!!--- */
                P(sem);
                
                // printf("worker5 - numjobs in queue : %d , pid: %d\n", shaddr->count, getpid());
                
                sleep(rand()%4);
                
                if((shaddr->count > 1) && ((shaddr->cjQueue[shaddr->front].status & 0xF) != 0xF) && (((shaddr->cjQueue[shaddr->front].status >> 4) & 0xFF) == 0xFF) && (((shaddr->cjQueue[(shaddr->front+1)%QUEUE_SZ].status >> 4) & 0xFF) == 0xFF))
                {              
                    is_working = 1;
                    // printf("worker6 - numjobs in queue : %d , pid: %d\n", shaddr->count, getpid());
                    computing_job *A = &(shaddr->cjQueue[shaddr->front]);
                    A->workergrp = shaddr->wgrp;
                    computing_job *B = &(shaddr->cjQueue[(shaddr->front+1)%QUEUE_SZ]);
                    B->workergrp = shaddr->wgrp;
                    
                    /* If this is the first worker */
                    if((A->status & 0xF) == 0b0010){
                        /* When there is space in queue and jobs are left to be produced, produce job */
                        int matid = 1 + rand_idx.get(); /* Random index between 1 and 100000 */
                        computing_job C(shaddr->wgrp, 0x002, matid, shaddr->wgrp);
                        shaddr->cjQueue[shaddr->back] = C;
                        shaddr->back = (shaddr->back + 1)%QUEUE_SZ;
                        shaddr->count += 1;
                    }

                    set_block_access_status(&(A->status));
                    
                    copy_block(A_block, A->mat, (A->status >> 2));
                    copy_block(B_block, B->mat, A->status);
                    A_wgrp = A->workergrp;
                    A_status = A->status;

                    // cout<<"A_status : %x"<<A_status<<"at pid "<<getpid()<<"\n";
                    printf("Copied matrix %d A block num %d to local at pid: %d \n", A->matid, ((A->status >> 2) & 0b0011), getpid());
                    printf("Copied matrix %d B block num %d to local at pid: %d \n", B->matid, (A->status & 0b0011), getpid());
                    printf("Number of jobs in queue : %d, Worker group of this worker : %d\n", shaddr->count, shaddr->wgrp);
                    
                    if((A->status & 0xF) == 0xF){
                        shaddr->front = (shaddr->front+2)%QUEUE_SZ;
                        shaddr->count -= 2;
                        shaddr->wgrp++;
                    }
                }
                // printf("outside lock : A_status : %x at pid %d\n", A_status, getpid());
                // sleep(rand()%4);
                /* ---!!! Critical Section End !!!--- */

                if(is_working){
                    /* Search for the corresponding C matrix in the queue */
                    computing_job *C;
                    int flag=0;
                    for(int i = 0; i < QUEUE_SZ; i++){
                        if((((shaddr->cjQueue[i].status >> 4) & 0xFF) != 0xFF) && (shaddr->cjQueue[i].workergrp == A_wgrp)){
                            C = &(shaddr->cjQueue[i]);
                            // for(int i=0;i<N;i++){
                            //     for(int j=0;j<N;j++){
                            //         printf("%d ", C->mat[i][j]);
                            //     }
                            //     printf("\n");
                            // }
                            flag=1;
                            break;
                        }
                    }
                    if(flag==0){
                        printf("Corresponding C matrix not found in job queue\n");
                    }
                    /* Do multiplication */
                    /* Add to corresponding C matrix */
                    int blocknum = ((A_status & 0b1000) >> 2) + (A_status & 0b0001);
                    // printf("A_status : %x blocknum : %d at pid %d\n", A_status, blocknum, getpid());
                    int copied = ((C->status >> (4 + blocknum)) & 1);
                    // printf("before prod : %x\n", C->status >> 4);
                    mat_product(C->mat, copied, blocknum, A_block, B_block);
                    if(copied){
                        C->status = (C->status | (1 << (blocknum + 8)));
                    }
                    else{
                        C->status = (C->status | (1 << (blocknum + 4)));
                    }
                    // printf("after prod : %x\n", C->status >> 4);
                }
                V(sem);
            }      
        }
    }
    // printf("parent1 - count : %d\n", shaddr->count);
    
    /* Wait if all jobs have not been pushed to the queue or if only one job is not remaining */
    while(!(shaddr->jobCounter == MATS && shaddr->count == 1));
    
    // printf("parent2 - count : %d\n", shaddr->count);

    /* Wait if the computation for the last remaining matrix is not complete yet */
    while(((shaddr->cjQueue[shaddr->front].status >> 4) & 0xFF) != 0xFF);

    // printf("parent3 - count : %d\n", shaddr->count);

    /* Calculate the sum of the principal diagonal of the resultant matrix */
    int trace = 0;
    for(int i=0;i<N;i++){
        trace += shaddr->cjQueue[shaddr->front].mat[i][i];
    }

    P(sem);
    printf("Sum of elements in principal diagonal is %d\n", trace);
    V(sem);

    waitpid(-1, NULL, 0);
    shmdt(shaddr);
    shmctl(shmid, IPC_RMID, 0);
    sem_unlink("/mutex7");
    sem_close(sem);
    printf("Parent w/ PID %d Exitted\n", getpid());
    sleep(10);
    return 0;
}

