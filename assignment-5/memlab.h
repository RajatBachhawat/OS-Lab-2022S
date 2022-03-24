#ifndef MEMLAB_H
#define MEMLAB_H

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <pthread.h>
#include <utility>
#include "avl.h"
#include "avl.cpp"
#include "string.h"

const int MAX_NAME_SIZE = 32;
const int PAGE_TABLE_SIZE = 10000;
const int LARGE_HOLE_SIZE = 5120;

enum DataType {
    Int, MediumInt, Boolean, Char
};

struct FreeInterval {
    int start;
    int end;
};

struct FreeSpace {
    unsigned int spaceSize;
    int start;
};

class PageTableEntry {
public:
    PageTableEntry();
    ~PageTableEntry();
    PageTableEntry &operator=(const PageTableEntry &other);
    char name[MAX_NAME_SIZE];
    DataType type;
    int localAddr; // TODO : int or long long?
    int logicalAddr;
    int scope;
    int num_elements;
    pthread_mutex_t mutexLock;
};

class PageTable {
public:
    int numfields = 0;
    PageTableEntry ptEntry[PAGE_TABLE_SIZE];
};


class BookkeepingMem {
public:
    BookkeepingMem();
    ~BookkeepingMem();
    PageTable pageTable;
    AVLTree<pair<int, int>> freeIntervals;
    AVLTree<pair<int, int>> freeSpaces;
    pthread_mutex_t compactLock;
    int findBestFitFreeSpace(unsigned int size);
};

void createMem(int memSize);
PageTableEntry *createVar(char varName[32], DataType type);
void assignVar(PageTableEntry* ptr, int value);
int varValue(PageTableEntry* ptr);
PageTableEntry* createArr(char arrName[32], DataType type, int elements);
void assignArr(PageTableEntry* ptr, int index, int value);
int arrValue(PageTableEntry* ptr, int index);
void freeElement(PageTableEntry* ptr);

void gcInit();
void gcStop();
void *gcRunner(void *param);
void gcRun(int opt);
void endScope();
void startScope();
void compactMem();
void printPageTable();

#endif