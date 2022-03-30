#ifndef MEMLAB_H
#define MEMLAB_H

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <utility>
#include <algorithm>
#include "avl.h"
#include "avl.cpp"
#include "string.h"

const int MAX_NAME_SIZE = 32;
const int PAGE_TABLE_SIZE = 10000;
const int LARGE_HOLE_SIZE = 5120;

enum DataType {
    Int, MediumInt, Char, Boolean
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
    int numElements;
    pthread_mutex_t mutexLock;
};

class PageTable {
public:
    int numEntries = 0;
    PageTableEntry pteList[PAGE_TABLE_SIZE];
};


class BookkeepingMem {
public:
    BookkeepingMem();
    ~BookkeepingMem();
    /* Page table (symbol table) */
    PageTable pageTable;
    /* Set (AVL tree implementation) for storing free intervals of memory in ascending order
     * pair<int, int> : <start index, end index>
     */
    AVLTree<pair<int, int>> freeIntervals;
    /* Set (AVL tree implementation) for storing free spaces in memory in ascending order of size
     * pair<int, int> : <space, start index>
     */
    AVLTree<pair<int, int>> freeSpaces;
    /* Starting address of memory created with createMem(). This is what we always use to access the memory
     * logicalMem is a unsigned int * because we always access memory in chunks of 4 bytes (word alignment)
     */
    unsigned int *logicalMem;
    /* Global counter for local address */
    unsigned int localAddrCounter = 0;
    /* Global scope counter */
    int currScope = 0;
    /* Size of memory allocated */
    int sizeOfMem = 0;
    /* Free space left */
    int freeMem = 0;
    /* Thread ID of the garbage collector thread */
    pthread_t gcTid;
    /* Mutex lock to be used during compaction done by garbage collector */
    pthread_mutex_t compactLock;
    /* Mutex lock to be used for printing log messages */
    pthread_mutex_t loggingLock;
    /* Time of start */
    timeval st;
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
void freeElementMem(PageTableEntry* ptr, int destroy);
void copyBlock(int sz, int olda, int newa);
void diagnose();
int Pthread_mutex_lock(pthread_mutex_t *__mutex);
int Pthread_mutex_unlock(pthread_mutex_t *__mutex);

void gcInit();
void gcStop();
void *gcRunner(void *param);
void gcRun(int opt);
void endScope();
void startScope();
void compactMem();
void printPageTable();

#endif