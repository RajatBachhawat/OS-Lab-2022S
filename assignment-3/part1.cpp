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
using namespace std;

typedef struct _process_data {
double **A;
double **B;
double **C;
int veclen, i, j;
} ProcessData;

void *mult(void *arg)
{
   ProcessData * pd = (ProcessData*)arg;
   pd->C[pd->i][pd->j] = 0.0;
   for(int k = 0; k<pd->veclen;k++)
   {
      pd->C[pd->i][pd->j]+=pd->A[pd->i][k]*pd->B[k][pd->j];
   }  
   return arg;
}

unsigned int matrixSpace(size_t sz, int rows, int cols)
{
   return rows*(sizeof(void*)+cols*sz);
}

void assignSpace(void** mat, int rows, int cols, size_t sz)
{
   mat[0] = mat+rows;
   for(int i=1; i<rows;i++)
   {
      mat[i]=mat[i-1]+(sz*cols);
   }
}

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
   int r1,c1,r2,c2;
   double **A,**B;
   int i,j;
   int shmid_A, shmid_B, shmid_C;
   
   cout<<"Enter dimensions of matrix A: ";
   cin>>r1>>c1;
   shmid_A = shmget(SHM_KEY, matrixSpace(sizeof(double),r1,c1), IPC_CREAT|0600); 
   A = (double**)shmat(shmid_A,NULL,0);
   // making matrix in shared mem
   assignSpace((void**)A,r1,c1,sizeof(double));

   cout<<"Enter elements of matrix A\n";
   for(i=0;i<r1;i++)
   {
      for(j=0;j<c1;j++)
      {
        // cin>>A[i][j];
        A[i][j] = rand()%10 + 1;
      }
   }

   cout<<"Enter dimensions of matrix B: ";
   cin>>r2>>c2;
   shmid_B = shmget(SHM_KEY+1, matrixSpace(sizeof(double),r2,c2), IPC_CREAT|0600); 
   B = (double**)shmat(shmid_B,NULL,0);
   // making matrix in shared mem
   assignSpace((void**)B,r2,c2,sizeof(double));

   cout<<"Enter elements of matrix B\n";
   for(i=0;i<r2;i++)
   {
      for(j=0;j<c2;j++)
      {
         B[i][j] = rand()%10 + 1;
         // cin>>B[i][j];
      }
   }

   double** C;
   shmid_C = shmget(SHM_KEY+2, matrixSpace(sizeof(double),r1,c2), IPC_CREAT|0600); 
   C = (double**)shmat(shmid_C,NULL,0);
   // making matrix in shared mem
   assignSpace((void**)C,r1,c2,sizeof(double));
   
   if(c1!=r2)
   {
      cout<<"Matrices cannot be multiplied\n";
      exit(0);
   }
   cout<<"Matrix A:\n";
   // printMatrix(A,r1,c1);
   cout<<"Matrix B:\n";
   // printMatrix(B,r2,c2);

   for(i=0;i<r1;i++)
   {
      for(j=0;j<c2;j++)
      {
         if(fork()==0)
         {
            ProcessData pd = {A,B,C,r2,i,j};
            mult(&pd);
            exit(0);
         }
      }
   }
   pid_t waitpid;
   while ((waitpid = wait(NULL)) > 0);
   cout<<"Matrix C:\n";
   // printMatrix(C,r1,c2);

   // temporary function for checking
      auto checkfunc =  [&]() -> void 
      {
         double checker[r1][c2];
         for(i=0;i<r1;i++)
         {
            for(j=0;j<c2;j++)
            {
               checker[i][j]=0.0;
               for(int k=0;k<r2;k++)
               {
                  checker[i][j]+=A[i][k]*B[k][j];
               }
            }
         }
         bool f = true;
         for(i=0;i<r1;i++)
         {
            for(j=0;j<c2;j++)
            {
               if(abs(C[i][j]-checker[i][j])>1e-3)
               {
                  f=false;
                  //cout<<C[i][j]<<" "<<checker[i][j]<<", ";
               }
            }
         }
         if(f)
         cout<<"YES\n";
         else 
         cout<<"NO\n";
      };

      checkfunc();
      shmdt(C);
      shmctl(shmid_C, IPC_RMID, 0);
      shmdt(B);
      shmctl(shmid_B, IPC_RMID, 0);
      shmdt(A);
      shmctl(shmid_A, IPC_RMID, 0);
   
   return 0;
}

