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
               
               if(shaddr->jobCounter>=MATS)
               {
                   cout<<"exit\n";
                   exit(0);
               }
               while(shaddr->count == QUEUE_SZ);
              // P(&shaddr->mutex);
               computingJob cj(i,0,shaddr->jobCounter);
               cout<<"Added: "<<cj.matid<<" "<<shaddr->count<<"\n";
               shaddr->jobCounter+=1;
               shaddr->cjQueue[shaddr->back] = cj;
               shaddr->back = (shaddr->back+1)%QUEUE_SZ;
               shaddr->count+=1;
               //V(&shaddr->mutex);
           }
       }
   }
   for(int i=0; i<NW;i++)
   {
       if(fork()==0)
       {
           while(true)
           {
               while(shaddr->count == 0);
              // P(&shaddr->mutex);
               computingJob cj = shaddr->cjQueue[shaddr->front];
               shaddr->front = (shaddr->front+1)%QUEUE_SZ;
               cout<<"Matrix: "<<cj.matid<<" "<<shaddr->count<<"\n";
               shaddr->count-=1;
              // V(&shaddr->mutex);
           }
           
       }
   }
   sleep(100);
   shmdt(shaddr);
   shmctl(shmid, IPC_RMID, 0);
//    for(int i =0; i<QUEUE_SZ; i++)
//    {
//        shaddr->cjQueue[i].matid = i;
//        shaddr->cjQueue[i].producerNo = i;
//        shaddr->cjQueue[i].status = 0;
//        shaddr->cjQueue[i].mat[0][0] = rand()%10;
//        shaddr->cjQueue[i].mat[0][1] = rand()%10;
//        shaddr->cjQueue[i].mat[1][0] = rand()%10;
//        shaddr->cjQueue[i].mat[1][1] = rand()%10;
//    }
//    for(int i =0; i<QUEUE_SZ; i++)
//    {
//        cout<<shaddr->cjQueue[i].matid<<" ";
//        cout<<shaddr->cjQueue[i].producerNo<<" ";
//        cout<<shaddr->cjQueue[i].status<<"\n";
//        cout<<shaddr->cjQueue[i].mat[0][0]<<" ";
//        cout<<shaddr->cjQueue[i].mat[0][1]<<"\n";
//        cout<<shaddr->cjQueue[i].mat[1][0]<<" ";
//        cout<<shaddr->cjQueue[i].mat[1][1]<<"\n***\n";
//    }
   
   
   return 0;
}

