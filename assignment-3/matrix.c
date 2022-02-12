#include <stdio.h>
#include <stdlib.h>
#define N 6

void printMatrix(int **mat, int rows, int cols)
{
    for(int i=0; i<rows;i++)
    {
        for(int j=0;j<cols;j++)
        {
            printf("%d ",mat[i][j]);
        }
        printf("\n");
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

int main(){
    int mat[N][N];
    for(int i=0; i<N;i++)
    {
        for(int j=0; j<N;j++)
            mat[i][j] = -9 + rand()%19;
    }
    for(int i=0; i<N;i++)
    {
        for(int j=0; j<N;j++)
            printf("%d ", mat[i][j]);
        printf("\n");
    }
    int block[N/2][N/2];
    copy_block(block, mat, 0b11);
    for(int i=0; i<N/2;i++)
    {
        for(int j=0; j<N/2;j++)
            printf("%d ", block[i][j]);
        printf("\n");
    }
}