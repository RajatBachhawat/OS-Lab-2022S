#include <iostream>
#include "../memlab.h"

using namespace std;

int main(){
    createMem(4000000);
    gcInit();
    PageTableEntry *ptr[200];
    for(int i=0;i<200;i++){
        char var[32];
        sprintf(var, "arr%d", i);
        ptr[i] = createArr(var, Int, 2000);
        freeElement(ptr[i]);
        for(int j=0;j<200;j++){
            assignArr(ptr[i], j, j);
        }
        diagnose();
    }
    return 0;
}