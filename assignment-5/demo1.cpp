#include <iostream>
#include "memlab.h"

using namespace std;

int main(){

    createMem(200000);
    // gcInit()
    PageTableEntry *arr1 = createArr("arr1", Int, 20000);
    freeElement(arr1);
    PageTableEntry *arr2 = createArr("arr2", Int, 10000);
    gcRun(1);
    compactMem();
    PageTableEntry *arr3 = createArr("arr3", Int, 40000);
    // gcStop()

    return 0;
}