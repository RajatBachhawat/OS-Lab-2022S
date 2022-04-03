#include <iostream>
#include "../memlab.h"

using namespace std;

int main(){
    createMem(2500000);
    startScope();
    PageTableEntry *arr = createArr("arr", Int, 200000);
    endScope();
    PageTableEntry *p = createArr("p", MediumInt, 5);
    for(int i=0;i<5;i++){
        assignArr(p, i, i);
        cout<<arrValue(p, i)<<"\n";
    }
    freeElement(arr);
    gcRun(1);
    compactMem();
    for(int i=0;i<5;i++){
        cout<<arrValue(p, i)<<"\n";
    }
    return 0;
}