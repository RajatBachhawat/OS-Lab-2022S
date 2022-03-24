#include <iostream>
#include "memlab.h"

void fibonacci(PageTableEntry *k, PageTableEntry *fibArr){
    assignArr(fibArr, 0, 1);
    assignArr(fibArr, 1, 1);
    PageTableEntry *i = createVar("i", Int);
    printPageTable();
    assignVar(i, 2);
    while(varValue(i) <= varValue(k)){
        assignArr(fibArr, varValue(i), arrValue(fibArr, varValue(i) - 1) + arrValue(fibArr, varValue(i) - 2));
        assignVar(i, varValue(i) + 1);
    }
}

PageTableEntry *fibnacciProduct(PageTableEntry *k){
    PageTableEntry *fibArr = createArr("fibArr", Int, varValue(k) + 1);
    printPageTable();

    startScope();
    fibonacci(k, fibArr);
    endScope();
    printPageTable();
    gcRun(0);
    
    PageTableEntry *product = createVar("product", Int);
    printPageTable();
    assignVar(product, 1);
    PageTableEntry *j = createVar("j", Int);
    printPageTable();
    assignVar(j, 0);

    while(varValue(j) <= varValue(k)){
        assignVar(product, arrValue(fibArr, varValue(j)) * varValue(product));
        assignVar(j, varValue(j) + 1);
    }

    return product;
}

int main(int argc, char **argv){
    createMem(2000);
    startScope();
    gcInit();
    PageTableEntry *k = createVar("k", Int);
    printPageTable();
    assignVar(k, atoi(argv[1]));
    PageTableEntry *retval = createVar("retval", Int);
    printPageTable();
    startScope();
    PageTableEntry *product = fibnacciProduct(k);
    assignVar(retval, varValue(product));
    endScope();
    printPageTable();
    gcRun(0);
    printPageTable();
    cout << "Product of first " << varValue(k) << " fibonacci nos. = " << varValue(retval) << "\n";
    endScope();
    gcRun(0);
    return 0;
}