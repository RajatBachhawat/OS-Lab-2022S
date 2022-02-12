/* Filename: shm_write.c */
#include <stdio.h>
#include <iostream>
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

pthread_mutex_t count_mutex;

#define SHM_KEY 0x1234
#define SEM_KEY 0x54321
#define QUEUE_SZ 8
#define N 2

using namespace std;

void P(sem_t *sem)
{
     if (sem_wait(sem) < 0)
          perror("P error");
}

void V(sem_t *sem)
{
     if (sem_post(sem) < 0)
          perror("V error");
}

struct computingJob 
{
    int producerNo;     /* ID of producer that produced this job */
    int status;         /* Rightmost 12 bits are important
                         * copy(4)-access1(4)-access2(4)
                         */
    int matid;          /* Matrix ID */
    int mat[N][N];      /* Matrix */
    computingJob(int pn, int s, int m):producerNo(pn),status(s),matid(m)
    {
        for(int i=0; i<N;i++)
        {
            for(int j=0; j<N;j++)
                mat[i][j] = -9 + rand()%19;
        }
    }
};

struct randomID {
    int arrsize;
    int arr[100000];
    void init(){
        arrsize = 100000;
        for(int i=0;i<100000;i++){
            arr[i]=i;
        }
    }
    int get(){
        int idx = rand()%arrsize;
        int tmp = arr[idx];
        arr[idx] = arr[arrsize-1];
        arr[arrsize-1] = tmp;
        arrsize--;
        return tmp;
    }
};

struct sharedMem      
{
    int jobCounter;
    int front;
    int back;
    int count;
    computingJob cjQueue[QUEUE_SZ];
};

void printMatrix(double** mat, int rows, int cols)
{
    for(int i=0; i<rows;i++)
    {
        for(int j=0;j<cols;j++)
        {
            cout<<mat[i][j]<<" ";
        }
        cout<<"\n";
    }
}

void set_block_access_status(int *status){
    if((*status & 0b1111) == 0b0010){
        *status = 0b0000;
    }
    else if((*status & 0b1111) == 0b0000){
        *status = 0b0001;
    }
    else if((*status & 0b1111) == 0b0001){
        *status = 0b0110;
    }
    else if((*status & 0b1111) == 0b0110){
        *status = 0b0111;
    }
    else if((*status & 0b1111) == 0b0111){
        *status = 0b1000;
    }
    else if((*status & 0b1111) == 0b1000){
        *status = 0b1001;
    }
    else if((*status & 0b1111) == 0b1001){
        *status = 0b01110;
    }
    else if((*status & 0b1111) == 0b1110){
        *status = 0b1111;
    }
}

void copy_block(int dest[][N/2], int source[][N], int blocknum){
    int st_row = ((blocknum >> 1) & 1) ? N/2 : 0;
    int en_row = ((blocknum >> 1) & 1) ? N-1 : N/2-1;
    int st_col = (blocknum & 1) ? N/2 : 0;
    int en_col = (blocknum & 1) ? N -1 : N/2-1;

    for(int i=st_row, x=0; i<=en_row; i++, x++){
        for(int j=st_col, y=0; j<=en_col; j++, y++){
            dest[x][y] = source[i][j];
        }
    }
}

int main(int argc, char *argv[]) 
{
    srand(0);
    int NP,NW,MATS;
    cout<<"Number of Producers: ";
    cin>>NP;
    cout<<"Number of Workers: ";
    cin>>NW;
    cout<<"Number of Matrices: ";
    cin>>MATS;

    int shmid;
    sharedMem* shaddr;

    shmid = shmget(SHM_KEY, sizeof(sharedMem), IPC_CREAT | 0644); 
    shaddr = (sharedMem*)shmat(shmid,NULL,0);

    sem_t* sem;
    sem = sem_open("/semap", O_CREAT, 0644, 1); 
    cout<<errno<<" "<<sem<<"\n";
    shaddr->front = 0;
    shaddr->back = 0;
    shaddr->jobCounter = 0;
    shaddr->count = 0;
    randomID rand_idx;
    rand_idx.init();

    for(int i=0; i<NP; i++)
    {
        pid_t pid = fork ();
        if (pid < 0) {
            /* check for error  */
            sem_unlink ("/semap");   
            sem_close(sem);
            /* unlink prevents the semaphore existing forever */
            /* if a crash occurs during the execution         */
            printf("Fork error.\n");
        }
        else if(pid == 0)
        {
            while(1)
            {
                if(shaddr->jobCounter >= MATS)
                {
                    /* If MATS number of jobs have been added, stop producer */
                    cout<<"producer exit.\n";
                    exit(0);
                }

                /* Wait when queue is full */
                while(shaddr->count == QUEUE_SZ);

                P(sem);

                if(shaddr->count < QUEUE_SZ && shaddr->jobCounter < MATS){
                    /* When there is space in queue and jobs are left to be produced, produce job */
                    int matid = 1 + rand_idx.get(); /* Random index between 1 and 100000 */
                    computingJob cj(i, 0b0010, matid);
                    
                    sleep(rand()%4);

                    shaddr->jobCounter+=1;
                    shaddr->cjQueue[shaddr->back] = cj;
                    shaddr->back = (shaddr->back + 1)%QUEUE_SZ;
                    shaddr->count+=1;
                    cout<<"Computing Job Added\nProducer No:"<<cj.producerNo<<" Pid:"<<getpid()<<" Matrix No:"<<cj.matid<<"\n";
                }
                
                V(sem);
            }
            exit(0);
        }
    }
    for(int i=0; i<NW; i++)
    {
        pid_t pid = fork ();
        if (pid < 0) {
            /* check for error  */
            sem_unlink ("/semap");   
            sem_close(sem);  
            /* unlink prevents the semaphore existing forever */
            /* if a crash occurs during the execution         */
            printf("Fork error.\n");
        }
        else if(pid == 0)
        {
            while(1)
            {
                if(shaddr->jobCounter == MATS && shaddr->count <= 1)
                {
                   cout<<"consumer exit \n"<<getpid();
                   exit(0);
                }
                sleep(rand()%4);
                
                /* Wait if there is only one job in the queue */
                while(shaddr->count == 1);

                /* Wait for next job if all blocks have been accessed 2 times */
                int status_front = shaddr->cjQueue[shaddr->front].status;
                while((status_front & 0xF) == 0xF);
                
                int A_block[N/2][N/2], B_block[N/2][N/2];
                
                /* !!! Critical Section Start !!! */
                P(sem);

                sleep(rand()%4);
                // cout<<"Consumer "<<i<<"\n";
                if(shaddr->count > 1 && (status_front & 0xF) != 0xF)
                {              
                    computingJob *A = &(shaddr->cjQueue[shaddr->front]);
                    computingJob *B = &(shaddr->cjQueue[(shaddr->front+1)%QUEUE_SZ]);
                    
                    /* If this is the first worker */
                    if((A->status & 0xF) == 0b0010){
                        /* When there is space in queue and jobs are left to be produced, produce job */
                        int matid = 1 + rand_idx.get(); /* Random index between 1 and 100000 */
                        computingJob C(i, 0b0010, matid);
                        shaddr->cjQueue[shaddr->back] = C;
                        shaddr->back = (shaddr->back + 1)%QUEUE_SZ;
                        shaddr->count += 1;
                    }

                    set_block_access_status(&(A->status));
                    status_front = A->status;
                    
                    copy_block(A_block, A->mat, (status_front >> 2));
                    copy_block(B_block, B->mat, status_front);

                    cout<<"Copied matrix "<<A->matid<<"A block num "<< ((status_front >> 2) & 0b0011) <<" to local at pid:"<<getpid()<<"\n";
                    cout<<"Copied matrix "<<B->matid<<"B block num "<< (status_front & 0b0011) <<" to local at pid:"<<getpid()<<"\n";
                    cout<<"count "<<shaddr->count<<"\n";
                    
                    if((A->status & 0xF) == 0xF){
                        shaddr->front = (shaddr->front+2)%QUEUE_SZ;
                        shaddr->count -= 2;
                    }
                }

                V(sem);
                /* !!! Critical Section End !!! */
            }      
        }
    }
    
    while(!(shaddr->jobCounter == MATS && shaddr->count <= 1));
    shmdt(shaddr);
    shmctl(shmid, IPC_RMID, 0);
    sem_unlink("/semap");
    sem_close(sem);
    cout<<"End\n";
    sleep(10);
    return 0;
}

