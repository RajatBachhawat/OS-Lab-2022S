/* Filename: shm_write.c */
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

#define SHM_KEY 0x1234

/* ProcessData struct */
typedef struct _process_data {
    double **A;
    double **B;
    double **C;
    int veclen, i, j;
} ProcessData;

/* Multiply i-th row with j-th column and store in C{i,j} */
void *mult(void *arg)
{
    ProcessData * pd = (ProcessData*)arg;
    pd->C[pd->i][pd->j] = 0.0;
    for(int k = 0; k<pd->veclen; k++)
    {
        pd->C[pd->i][pd->j]+=pd->A[pd->i][k]*pd->B[k][pd->j];
    }  
    return arg;
}

/* Calculate space required to store the matrix */
unsigned int matrixSpace(size_t sz, int rows, int cols)
{
    return rows*(sizeof(void*)+cols*sz);
}

/* Assign pointers their appropriate addresses in 2D matrix */
void assignSpace(void** mat, int rows, int cols, size_t sz)
{
    mat[0] = mat + rows;
    for(int i=1; i<rows;i++)
    {
        mat[i] = mat[i-1] + (sz*cols);
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

/* Print the given 2D matrix */
void printMatrix(double** mat, int rows, int cols)
{
    for(int i=0; i<rows;i++)
    {
        for(int j=0;j<cols;j++)
        {
            printf("%lf ", mat[i][j]);
        }
        printf("\n");
    }
}

/* Returns a random double between low and high */
double drand(double low, double high)
{
    return ((double)rand() * (high - low))/(double)RAND_MAX + low;
}

/* Check if C stores the product AB */
void checkfunc(double **A, double **B, double **C, int r1, int r2, int c1, int c2){
    double checker[r1][c2];
    for(int i=0;i<r1;i++)
    {
        for(int j=0;j<c2;j++)
        {
            checker[i][j]=0.0;
            for(int k=0;k<r2;k++)
            {
                checker[i][j]+=A[i][k]*B[k][j];
            }
        }
    }
    bool f = true;
    for(int i=0;i<r1;i++)
    {
        for(int j=0;j<c2;j++)
        {
            if(abs(C[i][j]-checker[i][j])>1e-3)
            {
                f=false;
                //printf(C[i][j]<<" "<<checker[i][j]<<", ";
            }
        }
    }
    if(f)
        printf("YES\n");
    else 
        printf("NO\n");
}

int main(int argc, char *argv[]) 
{
    int r1,c1,r2,c2;
    double **A,**B;
    int i,j;
    int shmid_A, shmid_B, shmid_C;
    
    printf("Enter dimensions of matrix A: ");
    scanf("%d %d", &r1, &c1);
    shmid_A = shmget(SHM_KEY, matrixSpace(sizeof(double), r1, c1), IPC_CREAT | 0600); 
    A = (double**)shmat(shmid_A, NULL, 0);
    /* Making matrix A in shared mem */
    assignSpace((void**)A, r1, c1, sizeof(double));

    printf("Enter elements of matrix A:\n");
    for(i=0;i<r1;i++)
    {
        for(j=0;j<c1;j++)
        {
            scanf("%lf", &(A[i][j]));
        }
    }

    printf("Enter dimensions of matrix B: ");
    scanf("%d %d", &r2, &c2);
    shmid_B = shmget(SHM_KEY+1, matrixSpace(sizeof(double), r2, c2), IPC_CREAT | 0600); 
    B = (double**)shmat(shmid_B, NULL, 0);
    /* Making matrix B in shared mem */
    assignSpace((void**)B, r2, c2, sizeof(double));

    printf("Enter elements of matrix B:\n");
    for(i=0;i<r2;i++)
    {
        for(j=0;j<c2;j++)
        {
            scanf("%lf", &(B[i][j]));
        }
    }

    double** C;
    shmid_C = shmget(SHM_KEY+2, matrixSpace(sizeof(double), r1, c2), IPC_CREAT | 0600); 
    C = (double**)shmat(shmid_C, NULL, 0);
    /* Making matrix C in shared mem */
    assignSpace((void**)C, r1, c2, sizeof(double));
    
    if(c1!=r2)
    {
        printf("Matrices cannot be multiplied\n");
        exit(0);
    }

    printf("Matrix A:\n");
    printMatrix(A,r1,c1);
    
    printf("Matrix B:\n");
    printMatrix(B,r2,c2);

    for(i=0; i<r1; i++)
    {
        for(j=0; j<c2; j++)
        {
            pid_t pid = fork();
            if(pid < 0){
                fprintf(stderr, "Fork error. errno %d: %s\n", errno, strerror(errno));
                exit(0);
            }
            else if(pid == 0)
            {
                /* Inside child */
                ProcessData pd = {A, B, C, r2, i, j};
                /* Caclulate C{i,j} in this process */
                mult(&pd);
                exit(0);
            }
        }
    }

    /* Wait for all children to terminate */
    pid_t pid;
    while ((pid = waitpid(-1, NULL, 0)) > 0);
    
    printf("Matrix C:\n");
    printMatrix(C, r1, c2);

    /* Check if multiplication was correct */
    // checkfunc(A, B, C, r1, r2, c1, c2);

    /* Detach all the three matrices from shared memory */
    shmdt(C);
    shmctl(shmid_C, IPC_RMID, 0);
    shmdt(B);
    shmctl(shmid_B, IPC_RMID, 0);
    shmdt(A);
    shmctl(shmid_A, IPC_RMID, 0);
    
    return 0;
}

