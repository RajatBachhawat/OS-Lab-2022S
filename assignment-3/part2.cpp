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


#define SHM_KEY 0x1234
#define QUEUE_SZ 8
#define MATRIX_DIM 2

using namespace std;

struct computingJob 
{
    int producerNo;
    int status;
    int blocksDone;
    double mat[MATRIX_DIM][MATRIX_DIM];
    int matid;
};

struct sharedMem     
{
    int jobCounter;
    int front;
    int back;
    computingJob cjQueue[QUEUE_SZ];
};



void initializeMatrix(double **&mat, int rows, int cols)
{
   mat = (double**)malloc(rows*(sizeof(double*)));
   for(int i=0;i<rows;i++)
   {
      mat[i]=(double*)malloc(cols*sizeof(double));
   }
}

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
   int shmid;
   sharedMem* shaddr;

   shmid = shmget(SHM_KEY, sizeof(sharedMem), IPC_CREAT|0600); 
   shaddr = (sharedMem*)shmat(shmid,NULL,0);
   shaddr->front = shaddr->front = shaddr->jobCounter = 0;

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

