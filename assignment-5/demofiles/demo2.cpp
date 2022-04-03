#include <iostream>
#include "../memlab.h"

void fibonacci(PageTableEntry *k, PageTableEntry *fibArr){
    assignArr(fibArr, 0, 1);
    assignArr(fibArr, 1, 1);
    PageTableEntry *i = createVar("i", Int);
    diagnose();
    // printPageTable();
    assignVar(i, 2);
    while(varValue(i) <= varValue(k)){
        assignArr(fibArr, varValue(i), arrValue(fibArr, varValue(i) - 1) + arrValue(fibArr, varValue(i) - 2));
        assignVar(i, varValue(i) + 1);
    }
}

PageTableEntry *fibnacciProduct(PageTableEntry *k){
    diagnose();
    PageTableEntry *fibArr = createArr("fibArr", Int, varValue(k) + 1);
    // printPageTable();

    startScope();
    fibonacci(k, fibArr);
    endScope();
    #ifdef GC_STACK_POP
    gcRun(0);
    #endif
    diagnose();
    
    PageTableEntry *product = createVar("product", Int);
    // printPageTable();
    assignVar(product, 1);
    PageTableEntry *j = createVar("j", Int);
    diagnose();
    // printPageTable();
    assignVar(j, 0);
    while(varValue(j) <= varValue(k)){
        assignVar(product, arrValue(fibArr, varValue(j)) * varValue(product));
        assignVar(j, varValue(j) + 1);
    }

    return product;
}

int main(int argc, char **argv){
    createMem(20000);

    #ifdef GC
    gcInit();
    #endif
    
    startScope();
    PageTableEntry *k = createVar("k", Int);
    // printPageTable();
    assignVar(k, atoi(argv[1]));
    PageTableEntry *retval = createVar("retval", Int);
    // printPageTable();
    diagnose();
    startScope();
    PageTableEntry *product = fibnacciProduct(k);
    assignVar(retval, varValue(product));
    endScope();
    // printPageTable();
    #ifdef GC_STACK_POP
    gcRun(0);
    #endif
    diagnose();
    // printPageTable();
    printf("Product of first %d fibonacci nos. = %d\n", varValue(k), varValue(retval));
    endScope();
    #ifdef GC_STACK_POP
    gcRun(0);
    #endif
    diagnose();
    return 0;
}