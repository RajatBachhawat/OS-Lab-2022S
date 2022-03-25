#include <iostream>
#include "memlab.h"

using namespace std;
int glob = 0;
void func1(PageTableEntry* ptr1, PageTableEntry* ptr2)
{
    startScope();
    char name[32];
    sprintf(name,"arr%d",glob);    
    PageTableEntry* arr = createArr(name,ptr1->type,50000);
    sprintf(name,"i%d",glob);
    glob++;
    PageTableEntry* i = createVar(name,Int);
    assignVar(i,0);
    for(;;)
    {
        assignArr(arr,varValue(i),rand()%1);
        assignVar(i,varValue(i)+1);
        if(varValue(i) == 50000)
            break;
    }
    endScope();
    gcRun(0);
}

int main(){

    createMem(250000000);
    
    // // With Int Values
    // PageTableEntry* p1 =  createVar("x",Int);
    // PageTableEntry* p2 =  createVar("y",Int);
    // func1(p1,p2);

    // // Medium Int
    // p1 = createVar("x1",MediumInt);
    // p2 =  createVar("y1",MediumInt);
    // func1(p1,p2);

    // // Char
    // p1 = createVar("x2",Char);
    // p2 =  createVar("y2",Char);
    // func1(p1,p2);

    // //Boolean 
    // p1 = createVar("x3",Boolean);
    // p2 =  createVar("y3",Boolean);
    // func1(p1,p2);

    // // With Int Values
    // p1 =  createVar("x4",Int);
    //  p2 =  createVar("y4",Int);
    // func1(p1,p2);

    // // Medium Int
    // p1 = createVar("x5",MediumInt);
    // p2 =  createVar("y5",MediumInt);
    // func1(p1,p2);

    // // Char
    // p1 = createVar("x6",Char);
    // p2 =  createVar("y6",Char);
    // func1(p1,p2);

    // //Boolean 
    // p1 = createVar("x7",Boolean);
    // p2 =  createVar("y7",Boolean);
    // func1(p1,p2);

    // // With Int Values
    // p1 =  createVar("x8",Int);
    // p2 =  createVar("y8",Int);
    // func1(p1,p2);

    // // Medium Int
    // p1 = createVar("x9",MediumInt);
    // p2 =  createVar("y9",MediumInt);
    // func1(p1,p2);

    // // Char
    // p1 = createVar("x10",Char);
    // p2 =  createVar("y10",Char);
    // func1(p1,p2);
    PageTableEntry *a1 = createArr("a1", Int, 20000);
    PageTableEntry *a2 = createVar("a2", Char);
    assignVar(a2, 27);
    PageTableEntry *a3 = createVar("a3", MediumInt);
    assignVar(a3, 27000);
    PageTableEntry *a4 = createVar("a4", MediumInt);
    assignVar(a4, 28000);
    PageTableEntry *a5 = createVar("a5", Char);
    assignVar(a5, 28);
    cout<<"Before\n";
    cout<<"a2: "<<varValue(a2)<<"\n";
    cout<<"a3: "<<varValue(a3)<<"\n";
    cout<<"a4: "<<varValue(a4)<<"\n";
    cout<<"a5: "<<varValue(a5)<<"\n";
    freeElement(a1);
    freeElement(a2);
    freeElement(a4);
    gcRun(1);
    cout<<"After\n";
    cout<<"a3: "<<varValue(a3)<<"\n";
    cout<<"a5: "<<varValue(a5)<<"\n";
    compactMem();
    cout<<"After compactmem()\n";
    cout<<"a3: "<<varValue(a3)<<"\n";
    cout<<"a5: "<<varValue(a5)<<"\n";
    return 0;
}