/* Filename: shm_write.c */
#include<stdio.h>
#include<iostream>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<sys/types.h>
#include<string.h>
#include<errno.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include <wait.h>
#include <pthread.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <semaphore.h>

pthread_mutex_t count_mutex;


#define SHM_KEY 0x1234
#define SEM_KEY 0x54321
#define QUEUE_SZ 8
#define MATRIX_DIM 2

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
    int producerNo;
    int status;
    int matid;
    double mat[MATRIX_DIM][MATRIX_DIM];
    computingJob(int pn, int s, int m):producerNo(pn),status(s),matid(m)
    {
        for(int i=0; i<MATRIX_DIM;i++)
        {
            for(int j=0; j<MATRIX_DIM;j++)
                mat[i][j]=-9+rand()%19;
        }
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

   shmid = shmget(SHM_KEY, sizeof(sharedMem), IPC_CREAT|0600); 
   shaddr = (sharedMem*)shmat(shmid,NULL,0);

   sem_t* sem;
   sem = sem_open ("hang", O_CREAT , 0644, 1); 
   cout<<errno<<"\n";
   shaddr->front =0;
   shaddr->back = 0;
   shaddr->jobCounter = 0;
   shaddr->count =0;
   //sem_init(&shaddr->mutex,0,0);

   for(int i=0; i<NP;i++)
   {
       if(fork()==0)
       {
           while(true)
           {
               sleep(rand()%4);
               //sem_wait(sem);
               if(shaddr->jobCounter>=MATS)
               {
                   cout<<"exit\n";
                   exit(0);
               }
               while(shaddr->count == QUEUE_SZ);
               computingJob cj(i,0,shaddr->jobCounter);
               cout<<"Computing Job Added\nProducer No:"<<cj.producerNo<<" Pid:"<<getpid()<<" Matrix No:"<<cj.matid<<"\n";
               shaddr->jobCounter+=1;
               shaddr->cjQueue[shaddr->back] = cj;
               shaddr->back = (shaddr->back+1)%QUEUE_SZ;
               shaddr->count+=1;
               //sem_post(sem);
           }
       }
   }
   for(int i=0; i<NW;i++)
   {
       if(fork()==0)
       {
           while(true)
           {
               sleep(rand()%4);
               
               
               
               while(shaddr->count == 1);    
               sem_wait(sem);
               cout<<"Consumer "<<i<<"\n";
               sleep(rand()%4);
               if(shaddr->count>1)
               {           
                computingJob cj = shaddr->cjQueue[shaddr->front];
                shaddr->front = (shaddr->front+1)%QUEUE_SZ;
                cout<<"Computed matrix "<<cj.matid<<" at pid:"<<getpid()<<"\n";
                shaddr->count-=1;
               }
               sem_post(sem);
           }
           
       }
   }
   
   while(!(shaddr->jobCounter == MATS && shaddr->count<=1));

   shmdt(shaddr);
   shmctl(shmid, IPC_RMID, 0);
   exit(0);
   sleep(10);
   sem_unlink("hang");
   sem_close(sem);
   cout<<"End\n";
   sleep(10);

   
   return 0;
}

