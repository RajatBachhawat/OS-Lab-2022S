#include <iostream>
#include "../memlab.h"

using namespace std;

int main(){
    createMem(400000);
    PageTableEntry *a1 = createArr("a1", Int, 20000);
    PageTableEntry *a2 = createVar("a2", Char);
    assignVar(a2, 27);
    PageTableEntry *a3 = createVar("a3", MediumInt);
    assignVar(a3, 27000);
    PageTableEntry *a4 = createVar("a4", MediumInt);
    assignVar(a4, 28000);
    PageTableEntry *a5 = createVar("a5", Int);
    assignVar(a5, 3000002);
    PageTableEntry *a6 = createVar("a6", Char);
    assignVar(a6, 28);
    cout<<"Before Garbage Collection\n";
    cout<<"a2: "<<varValue(a2)<<"\n";
    cout<<"a3: "<<varValue(a3)<<"\n";
    cout<<"a4: "<<varValue(a4)<<"\n";
    cout<<"a5: "<<varValue(a5)<<"\n";
    cout<<"a6: "<<varValue(a6)<<"\n";
    freeElement(a1);
    freeElement(a2);
    freeElement(a4);
    gcRun(1);
    cout<<"After Garbage Collection\n";
    cout<<"a3: "<<varValue(a3)<<"\n";
    cout<<"a5: "<<varValue(a5)<<"\n";
    cout<<"a6: "<<varValue(a6)<<"\n";
    compactMem();
    cout<<"After compactmem()\n";
    cout<<"a3: "<<varValue(a3)<<"\n";
    cout<<"a5: "<<varValue(a5)<<"\n";
    cout<<"a6: "<<varValue(a6)<<"\n";
    return 0;
}
