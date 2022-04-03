#include "memlab.h"

BookkeepingMem *bk; /* Bookkeeping data structures */

static unsigned int typeSize[4] = {4, 3, 1, 1};
static unsigned char typeName[4][10] = {"Int", "MediumInt", "Char", "Boolean"};

int Pthread_mutex_lock(pthread_mutex_t *__mutex){
    int retval = pthread_mutex_lock(__mutex);
    if(retval < 0){
        fprintf(stderr, "Pthread_mutex_lock error: %s", strerror(errno));
    }
    return retval;
}
int Pthread_mutex_unlock(pthread_mutex_t *__mutex){
    int retval = pthread_mutex_unlock(__mutex);
    if(retval < 0){
        fprintf(stderr, "Pthread_mutex_unlock error: %s", strerror(errno));
    }
    return retval;
}

/* BookkeepingMem constructor */
BookkeepingMem::BookkeepingMem(){
    pthread_mutexattr_t attr;
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
    /* Initialise mutex lock for compaction */
    pthread_mutex_init(&(this->compactLock), &attr);
    /* Initialise mutex lock for logging */
    pthread_mutex_init(&(this->loggingLock), &attr);
    pthread_mutexattr_destroy(&attr);
}

/* BookkeepingMem destructor */
BookkeepingMem::~BookkeepingMem(){
    pthread_mutex_destroy(&(this->compactLock));
    pthread_mutex_destroy(&(this->loggingLock));
}

/* PageTableEntry constructor */
PageTableEntry::PageTableEntry(){
    pthread_mutexattr_t attr;
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
    /* Initialise mutex lock for reading/writing the scope of a page table entry */
    pthread_mutex_init(&(this->scopeLock), &attr);
    /* Initialise mutex lock for the logical address of the variable/array */
    pthread_mutex_init(&(this->addrLock), &attr);
    pthread_mutexattr_destroy(&attr);
}

/* PageTableEntry destructor */
PageTableEntry::~PageTableEntry(){
    pthread_mutex_destroy(&(this->scopeLock));
}

/* PageTableEntry copy assignment operator */
PageTableEntry &PageTableEntry::operator=(const PageTableEntry &other){
    if(this != &other){
        memcpy(this->name, other.name, MAX_NAME_SIZE);
        this->type = other.type;
        this->localAddr = other.localAddr;
        this->logicalAddr = other.logicalAddr;
        this->scope = other.scope;
    }
    return *this;
}

/* PageTableEntry output streaming operator */
ostream &operator<<(ostream &op, const PageTableEntry &pte){
    op << pte.name << ", " << pte.scope << "\n";
    return op;
}

/* Finds the best-fit free space in memory. In other words, smallest free space with size >= reqd size */
int findBestFitFreeSpace(unsigned int size){
    /* Find the smallest free space greater than size in freeSpaces */
    Node<pair<int,int>>* _fslb = bk->freeSpaces.lower_bound({size, -1});
    if(_fslb == nullptr)
    {
        return -1;
    }
   
    pair<int, int> fslb = _fslb->data;

    /* Find the corresponding interval in freeIntervals */
    pair<int, int> filb = bk->freeIntervals.lower_bound({fslb.second, -1})->data;


    pair<int, int> remWord = fslb;
    pair<int, int> remInterval = filb;

    /* Delete the existing intervals */
    bk->freeIntervals.remove(filb);
    bk->freeSpaces.remove(fslb);

    int totspace = fslb.first;
    int modspace = (totspace-size)%4;

    /* Inserting right half of remaining chunk */
    if(totspace-size-modspace !=0)
    {
        bk->freeSpaces.insert({totspace-size-modspace,remWord.second+size+modspace});
        bk->freeIntervals.insert({remWord.second+size+modspace,remWord.second+totspace});
    }

    /* Inserting left half of remaining chunk  */
    if(modspace!=0)
    {
        bk->freeSpaces.insert({modspace,remWord.second+size});
        bk->freeIntervals.insert({remWord.second+size,remWord.second+size+modspace});
    }

    return fslb.second;
}

void createMem(int memSize) {
    bk = new BookkeepingMem;
    bk->sizeOfMem = memSize;
    bk->freeMem = memSize;
    printf("- Allocated space for bookkeeping data structures\n");
    bk->freeSpaces.insert({memSize,0});
    bk->freeIntervals.insert({0,memSize});
    printf("- Initialized data structures\n");
    bk->logicalMem = (unsigned int *)malloc(memSize);
    printf("- Allocated space for required by user\n\n");
    gettimeofday(&bk->st, NULL);
}

PageTableEntry *createVar(char varName[32], DataType type){
    PageTable *pt = &(bk->pageTable);

    for(int ind = 0; ind<pt->numEntries; ind++)
    {
        if(strcmp(pt->pteList[ind].name,varName) == 0 && pt->pteList[ind].scope >= 0)
        {
            Pthread_mutex_lock(&bk->loggingLock);
            printf("Variable/Array with name \'%s\' already exists\n", varName);
            Pthread_mutex_unlock(&bk->loggingLock);
            exit(EXIT_FAILURE);
        }
    }

    Pthread_mutex_lock(&(bk->compactLock));

    int spaceAssignedInd = findBestFitFreeSpace(typeSize[type]);
    if(spaceAssignedInd == -1)
    {
        Pthread_mutex_lock(&bk->loggingLock);
        printf("No Space for Variable\n");
        Pthread_mutex_unlock(&bk->loggingLock);

        Pthread_mutex_unlock(&(bk->compactLock));
        exit(EXIT_FAILURE);
    }
    bk->freeMem -= typeSize[type];
    Pthread_mutex_lock(&bk->loggingLock);
    printf("- \'%s\' assigned %d bytes in memory\n\n", varName, typeSize[type]);
    Pthread_mutex_unlock(&bk->loggingLock);
    
    /* Add entry in page table */
    int idx = pt->numEntries;
    strcpy(pt->pteList[idx].name,varName);
    pt->pteList[idx].type = type;
    pt->pteList[idx].localAddr = bk->localAddrCounter;
    pt->pteList[idx].logicalAddr = spaceAssignedInd;
    pt->pteList[idx].scope = bk->currScope;
    pt->pteList[idx].numElements = 1;
    bk->localAddrCounter +=4;
    pt->numEntries++;

    Pthread_mutex_lock(&bk->loggingLock);
    printf("- Page table entry created for variable: \'%s\'\n", varName);
    printf("type : %s\n", typeName[type]);
    printf("local address : %d\n", bk->localAddrCounter - 4);
    printf("logical address : %d\n\n", spaceAssignedInd);
    Pthread_mutex_unlock(&bk->loggingLock);

    Pthread_mutex_unlock(&(bk->compactLock));

    return &pt->pteList[idx];
}

void assignValueInMem(int logicalAddr, int size, DataType type, int value)
{
    if((type == Boolean && (value !=0 && value!=1)))
    {
        Pthread_mutex_lock(&bk->loggingLock);
        printf("Value Being Assigned Cannot Fit In DataType\n");
        Pthread_mutex_unlock(&bk->loggingLock);
        exit(EXIT_FAILURE);
    }
    if(type != Boolean && (value >=(1ll<<(8*typeSize[type]-1))|| value < -(1ll<<(8*typeSize[type]-1))))
    {
        Pthread_mutex_lock(&bk->loggingLock);
        printf("Value Being Assigned Cannot Fit In DataType\n");
        Pthread_mutex_unlock(&bk->loggingLock);
        exit(EXIT_FAILURE);
    }

    unsigned int fourByteChunk = bk->logicalMem[logicalAddr/4];
    
    int startBit = 32-logicalAddr%4*8;
    int endBit = startBit - size*8;
    unsigned int mask = 0;
    mask+= ((1<<endBit)-1);
    mask+= ((1ll<<32ll)-1ll)-((1ll<<startBit)-1);
    unsigned int cpnum = value;
    cpnum = (cpnum<<(endBit));
    fourByteChunk&=mask;
    fourByteChunk|=cpnum;
    bk->logicalMem[logicalAddr/4] = fourByteChunk;

    Pthread_mutex_lock(&bk->loggingLock);
    printf("^ Word alignment : Wrote data in chunk of 4 bytes\n");
    Pthread_mutex_unlock(&bk->loggingLock);
}


void assignVar(PageTableEntry* ptr, int value)
{
    if(ptr == nullptr)
    {
        Pthread_mutex_lock(&bk->loggingLock);
        printf("Invalid Pointer\n");
        Pthread_mutex_unlock(&bk->loggingLock);
        exit(EXIT_FAILURE);
    }
    if(ptr->scope < 0){
        Pthread_mutex_lock(&bk->loggingLock);
        printf("Variable \'%s\'has gone out of scope\n", ptr->name);
        Pthread_mutex_unlock(&bk->loggingLock);
        exit(EXIT_FAILURE);
    }

    Pthread_mutex_lock(&bk->loggingLock);
    printf("^ Word alignment : Access memory for variable in chunk of 4 bytes\n");
    Pthread_mutex_unlock(&bk->loggingLock);

    Pthread_mutex_lock(&ptr->addrLock);
    int logAdd = ptr->logicalAddr;
    int size = typeSize[ptr->type];
    assignValueInMem(logAdd,size,ptr->type,value);
    Pthread_mutex_unlock(&ptr->addrLock);

    Pthread_mutex_lock(&bk->loggingLock);
    printf("- Assign value to variable\n");
    printf("%s := %d\n\n", ptr->name, value);
    Pthread_mutex_unlock(&bk->loggingLock);
}

int varValue(PageTableEntry* ptr)
{
    if(ptr==nullptr)
    {
        printf("Invalid Pointer\n");
        exit(EXIT_FAILURE);
    }
    if(ptr->scope < 0){
        Pthread_mutex_lock(&bk->loggingLock);
        printf("Variable \'%s\'has gone out of scope\n", ptr->name);
        Pthread_mutex_unlock(&bk->loggingLock);
        exit(EXIT_FAILURE);
    }
    Pthread_mutex_lock(&ptr->addrLock);
    int retval = 0;
    int logAdd = ptr->logicalAddr;
    int size = typeSize[ptr->type];
    unsigned int fourByteChunk = bk->logicalMem[logAdd/4];
    
    Pthread_mutex_lock(&bk->loggingLock);
    printf("^ Word alignment : Read \'%s\' in chunk of 4 bytes\n\n", ptr->name);
    Pthread_mutex_unlock(&bk->loggingLock);

    int startBit = 32-logAdd%4*8;
    int endBit = startBit - size*8;
    unsigned int mask = 0;
    mask+= ((1<<endBit)-1);
    mask+= ((1ll<<32ll)-1ll)-((1ll<<startBit)-1);
    mask^=((1ll<<32ll)-1ll);
    fourByteChunk&=mask;
    fourByteChunk = (fourByteChunk>>endBit);

    retval = fourByteChunk;

    if(size<4 && (fourByteChunk&(1ll<<(size*8ll-1))))
    {
        int allones = -1;
        allones = (allones << (size * 8));
        retval = (retval | allones);
    }
    Pthread_mutex_unlock(&ptr->addrLock);
    return retval;
}

PageTableEntry* createArr(char arrName[32], DataType type, int elements){
    PageTable *pt = &(bk->pageTable);

    for(int ind = 0; ind<pt->numEntries; ind++)
    {
        if(strcmp(pt->pteList[ind].name,arrName)==0)
        {
            Pthread_mutex_lock(&bk->loggingLock);
            printf("Variable/Array with name \'%s\' already exists\n", arrName);
            Pthread_mutex_unlock(&bk->loggingLock);
            exit(EXIT_FAILURE);
        }
    }
    int spaceneeded = 0;
    switch(type)
    {
        case Int: 
        case Char:
            spaceneeded = elements*typeSize[type];
            break;
        case MediumInt:
            if(elements == 1){
                spaceneeded = typeSize[type];
            }
            else{
                Pthread_mutex_lock(&bk->loggingLock);
                printf("^ Word alignment : MediumInt array elements are stored at offsets of 4\nSo space needed for array is 4 * (numElements)\n");
                Pthread_mutex_unlock(&bk->loggingLock);
                spaceneeded = elements*typeSize[Int];
            }
            break;
        case Boolean:
            spaceneeded = ((elements+7)/8);
            break;
    }
    Pthread_mutex_lock(&(bk->compactLock));
    int spaceAssignedInd = findBestFitFreeSpace(spaceneeded);
    if(spaceAssignedInd == -1)
    {   
        Pthread_mutex_lock(&bk->loggingLock);
        printf("No Space for Array\n");
        Pthread_mutex_unlock(&bk->loggingLock);

        Pthread_mutex_unlock(&(bk->compactLock));
        exit(EXIT_FAILURE);
    }
    bk->freeMem -= spaceneeded;
    Pthread_mutex_lock(&bk->loggingLock);
    printf("- \'%s\' array assigned %d bytes (for %d elements) in memory\n", arrName, spaceneeded, elements);
    Pthread_mutex_unlock(&bk->loggingLock);

    int idx = pt->numEntries;
    strcpy(pt->pteList[idx].name,arrName);
    pt->pteList[idx].type = type;
    pt->pteList[idx].localAddr = bk->localAddrCounter;
    pt->pteList[idx].logicalAddr = spaceAssignedInd;
    pt->pteList[idx].scope = bk->currScope;
    pt->pteList[idx].numElements = elements;
    bk->localAddrCounter += 4*elements;
    pt->numEntries++;

    Pthread_mutex_lock(&bk->loggingLock);
    printf("- Page table entry created for variable: \'%s\'\n", arrName);
    printf("type : %s\n", typeName[type]);
    printf("local address : %d\n", bk->localAddrCounter - 4);
    printf("logical address : %d\n\n", spaceAssignedInd);
    printf("number of elements in array : %d\n\n", elements);
    Pthread_mutex_unlock(&bk->loggingLock);

    Pthread_mutex_unlock(&(bk->compactLock));

    return &(pt->pteList[idx]);
}

void assignArr(PageTableEntry* ptr, int index, int value)
{
    if(ptr==nullptr)
    {
        Pthread_mutex_lock(&bk->loggingLock);
        printf("Invalid Pointer\n");
        Pthread_mutex_unlock(&bk->loggingLock);
        exit(EXIT_FAILURE);
    }
    if(ptr->scope < 0){
        Pthread_mutex_lock(&bk->loggingLock);
        printf("Array \'%s\'has gone out of scope\n", ptr->name);
        Pthread_mutex_unlock(&bk->loggingLock);
        exit(EXIT_FAILURE);
    }
    if(index >= ptr->numElements){
        printf("Index out of bounds\n");
        exit(EXIT_FAILURE);
    }
    int offset  =0;
    DataType type = ptr->type;
    switch(type)
    {
        case Int:
        case Char:
            offset = typeSize[type];
            break;
        case MediumInt:
            offset = 4;
            break;
        
    }
    if(type == Boolean)
    {
        if(value !=0 && value!=1)
        {
            Pthread_mutex_lock(&bk->loggingLock);
            printf("Value Being Assigned Cannot Fit In DataType\n");
            Pthread_mutex_unlock(&bk->loggingLock);
            exit(EXIT_FAILURE);
        }
        Pthread_mutex_lock(&ptr->addrLock);
        int byteChunkInd = ptr->logicalAddr +  index/8;
        int FourByteChunkInd = (byteChunkInd/4);
        unsigned int byteChunk = bk->logicalMem[FourByteChunkInd];
        
        Pthread_mutex_lock(&bk->loggingLock);
        printf("^ Word alignment : Access the memory for \'%s\'[%d] in chunk of 4 bytes\n", ptr->name, index);
        Pthread_mutex_unlock(&bk->loggingLock);

        int bitPos = 32 - ((ptr->logicalAddr)%4*8 + index);
        if(((byteChunk&(1<<bitPos))>>bitPos)!=value)
        {
            byteChunk = byteChunk^(1<<bitPos);
        }
        bk->logicalMem[FourByteChunkInd] = byteChunk;

        Pthread_mutex_lock(&bk->loggingLock);
        printf("^ Word alignment : Wrote data in chunk of 4 bytes\n");
        Pthread_mutex_unlock(&bk->loggingLock);

        Pthread_mutex_unlock(&ptr->addrLock);
    }
    else 
    {
        Pthread_mutex_lock(&bk->loggingLock);
        printf("^ Word alignment : Access the memory for \'%s\'[%d] in chunk of 4 bytes\n", ptr->name, index);
        Pthread_mutex_unlock(&bk->loggingLock);
        
        Pthread_mutex_lock(&ptr->addrLock);
        assignValueInMem(ptr->logicalAddr+offset*index,typeSize[type],type,value);
        Pthread_mutex_unlock(&ptr->addrLock);
    }

    Pthread_mutex_lock(&bk->loggingLock);
    printf("- Assign value at array index\n");
    printf("%s[%d] := %d\n\n", ptr->name, index, value);
    Pthread_mutex_unlock(&bk->loggingLock);
}

int arrValue(PageTableEntry* ptr, int index)
{
    if(ptr==nullptr)
    {
        Pthread_mutex_lock(&bk->loggingLock);   
        printf("Invalid Pointer\n");
        Pthread_mutex_unlock(&bk->loggingLock);
        
        exit(EXIT_FAILURE);
    }
    if(ptr->scope < 0){
        Pthread_mutex_lock(&bk->loggingLock);
        printf("Array \'%s\'has gone out of scope\n", ptr->name);
        Pthread_mutex_unlock(&bk->loggingLock);
        exit(EXIT_FAILURE);
    }
    if(index >= ptr->numElements){
        printf("Index out of bounds\n");
        exit(EXIT_FAILURE);
    }
    int offset  =0;
    DataType type = ptr->type;
    switch(type)
    {
        case Int:
        case Char:
            offset = typeSize[type];
            break;
        case MediumInt:
            offset = 4;
            break;
    }
    if(type == Boolean)
    {
        Pthread_mutex_lock(&ptr->addrLock);
        int byteChunkInd = ptr->logicalAddr +  index/8;
        int FourByteChunkInd = (byteChunkInd/4);
        unsigned int byteChunk = bk->logicalMem[FourByteChunkInd];

        Pthread_mutex_lock(&bk->loggingLock);
        printf("^ Word alignment : Read \'%s[%d]\' in chunk of 4 bytes\n\n", ptr->name, index);
        Pthread_mutex_unlock(&bk->loggingLock);
    
        int bitPos = 32 - ((ptr->logicalAddr)%4*8 + index);
        Pthread_mutex_unlock(&ptr->addrLock);
        return (byteChunk&(1<<bitPos))>>bitPos;
    }
    else 
    {
        Pthread_mutex_lock(&ptr->addrLock);
        int retval = 0;
        int logAdd = ptr->logicalAddr+offset*index;
        int size = typeSize[ptr->type];
        unsigned int fourByteChunk = bk->logicalMem[logAdd/4];

        Pthread_mutex_lock(&bk->loggingLock);
        printf("^ Word alignment : Read \'%s[%d]\' in chunk of 4 bytes\n\n", ptr->name, index);
        Pthread_mutex_unlock(&bk->loggingLock);

        int startBit = 32-logAdd%4*8;
        int endBit = startBit - size*8;
        unsigned int mask = 0;
        mask+= ((1<<endBit)-1);
        mask+= ((1ll<<32ll)-1ll)-((1ll<<startBit)-1);
        mask^=((1ll<<32ll)-1ll);
        fourByteChunk&=mask;
        fourByteChunk = (fourByteChunk>>endBit);


        retval = fourByteChunk;


        if(size<4 && (fourByteChunk&(1ll<<(size*8ll-1))))
        {
            int allones = -1;
            allones = (allones << (size * 8));
            retval = (retval | allones);
        }
        Pthread_mutex_unlock(&ptr->addrLock);
        return retval;
    }
}

void freeElementMem(PageTableEntry* ptr, int destroy)
{
    if(ptr==nullptr)
    {
        Pthread_mutex_lock(&bk->loggingLock);
        printf("Invalid Pointer\n");
        Pthread_mutex_unlock(&bk->loggingLock);
        exit(EXIT_FAILURE);
    }   
    
    int logicalAddr = ptr->logicalAddr;
    int space = 0;
    if(ptr->numElements == 1)
    {
        space = typeSize[ptr->type];
    }
    else 
    {
        switch(ptr->type)
        {
            case Int:
            case Char:
                space = typeSize[ptr->type] * ptr->numElements ;
                break;
            case MediumInt:
                space = typeSize[Int] * ptr->numElements;
                break;
            case Boolean:
                space = (ptr->numElements+7)/8;
                break;
        }
    }

    // Find entry in both sets
    
    pair<int,int>intervalEntry = {logicalAddr,logicalAddr+space};
    pair<int,int>spaceEntry = {space,logicalAddr};
    int start = intervalEntry.first;
    int end = intervalEntry.second;

    // Check if free space starts right after
    Node<pair<int,int>>* intervalEntryBefore = bk->freeIntervals.lower_bound({intervalEntry.second,-1});
    
    if(intervalEntryBefore!=nullptr && intervalEntryBefore->data.first == intervalEntry.second)
    {
        pair<int,int>pp = intervalEntryBefore->data;
        bk->freeIntervals.remove(pp);
        bk->freeSpaces.remove({pp.second-pp.first,pp.first});
        end = pp.second;
    }
    //Check if free space ends right before using binary search
    int ans = -1;
    int low = 0;
    int high = start;
    while(low<=high)
    {
        int mid = (low+high)/2;
        Node<pair<int,int>>*p= bk->freeIntervals.lower_bound({mid,-1});
        if(p==nullptr || p->data.first >= start)
        {
            high = mid-1;
        }
        else 
        {
            ans = mid;
            low = mid+1;
        }
    }
    // If the free interval  before current one ends at end
    if(ans!=-1 && bk->freeIntervals.lower_bound({ans,-1})->data.second == start )
    {
        start = ans;
        pair<int,int>p1 = bk->freeIntervals.lower_bound({ans,-1})->data;
        bk->freeIntervals.remove(p1);
        bk->freeSpaces.remove({p1.second-p1.first,p1.first});
    }


    if(1)
    {
        if(1)
        {
            pair<int,int>intervalEntry = {start,end};
            pair<int,int>spaceEntry = {end-start,start};
            // Check if free space starts right after
            if(end%4==0)
            {
            Node<pair<int,int>>* intervalEntryBefore = bk->freeIntervals.lower_bound({intervalEntry.second,-1});
            
            if(intervalEntryBefore!=nullptr && intervalEntryBefore->data.first == intervalEntry.second)
            {
                pair<int,int>pp = intervalEntryBefore->data;
                bk->freeIntervals.remove(pp);
                bk->freeSpaces.remove({pp.second-pp.first,pp.first});
                end = pp.second;
            }
            }
            if(start%4==0)
            {
            //Check if free space ends right before using binary search
            int ans = -1;
            int low = 0;
            int high = start;
            while(low<=high)
            {
                int mid = (low+high)/2;
                Node<pair<int,int>>*p= bk->freeIntervals.lower_bound({mid,-1});
                if(p==nullptr || p->data.first >= start)
                {
                    high = mid-1;
                }
                else 
                {
                    ans = mid;
                    low = mid+1;
                }
            }
            // If the free interval  before current one ends at end
            if(ans!=-1 && bk->freeIntervals.lower_bound({ans,-1})->data.second == start )
            {
                start = ans;
                pair<int,int>p1 = bk->freeIntervals.lower_bound({ans,-1})->data;
                bk->freeIntervals.remove(p1);
                bk->freeSpaces.remove({p1.second-p1.first,p1.first});
            }
           
            }
        }
    }
    if(start%4!=0) 
    {
        int extra = 4-start%4;
        bk->freeIntervals.insert({start+extra,end});
        bk->freeSpaces.insert({end-start-extra,start+extra});
        bk->freeIntervals.insert({start,start+extra});
        bk->freeSpaces.insert({extra,start});
    }
    else 
    {
        bk->freeSpaces.insert({end-start,start});
        bk->freeIntervals.insert({start,end});
    }

    bk->freeMem += space;
    if(destroy){
        Pthread_mutex_lock(&bk->loggingLock);
        printf("- Page table entry deleted (memory freed) for: \'%s\'\n\n", ptr->name);
        Pthread_mutex_unlock(&bk->loggingLock);
        ptr->scope = -2;
    }
    Pthread_mutex_lock(&bk->loggingLock);
    printf("Freed up Space between %d and %d\n", intervalEntry.first, intervalEntry.second);
    printf("Free Space Segment Created Between %d and %d\n\n", start, end);
    Pthread_mutex_unlock(&bk->loggingLock);

}

void freeElement(PageTableEntry *pte){
    /* Mark */
    Pthread_mutex_lock(&bk->loggingLock);
    printf("- Page table entry marked for deletion for: \'%s\'\n\n", pte->name);
    Pthread_mutex_unlock(&bk->loggingLock);

    pte->scope = -1;
}

void startScope(){
    bk->currScope++;
}

void endScope(){
    PageTable *pt = &(bk->pageTable);
    Pthread_mutex_lock(&bk->compactLock);
    for(int i = pt->numEntries - 1; i >= 0; i--){
        if(pt->pteList[i].scope == bk->currScope){
            Pthread_mutex_lock(&bk->loggingLock);
            printf("- Page table entry marked for deletion for: \'%s\'\n\n", pt->pteList[i].name);
            Pthread_mutex_unlock(&bk->loggingLock);

            pt->pteList[i].scope = -1;
        }
        else break;
    }
    bk->currScope--;
    Pthread_mutex_unlock(&bk->compactLock);
}

void gcRun(int opt){
    PageTable *pt = &(bk->pageTable);
    int i = pt->numEntries - 1;
    /* If running in main thread */
    if(opt == 0){
        Pthread_mutex_lock(&bk->compactLock);

        Pthread_mutex_lock(&bk->loggingLock);
        printf("***** Garbage collector will unwind the stack and free up corresponding memory *****\n\n");
        Pthread_mutex_unlock(&bk->loggingLock);

        if(i >= 0)
            Pthread_mutex_lock(&pt->pteList[i].scopeLock);
        while(i >= 0 && pt->pteList[i].scope < 0){
            if(pt->pteList[i].scope == -1){
                pt->pteList[i].scope = -2;
                Pthread_mutex_unlock(&pt->pteList[i].scopeLock);

                freeElementMem(&(pt->pteList[i]), 1);
            }
            else{
                Pthread_mutex_unlock(&pt->pteList[i].scopeLock);
            }

            pt->numEntries--;

            i--;
            if(i >= 0)
                Pthread_mutex_lock(&pt->pteList[i].scopeLock);
        }
        if(i >= 0)
            Pthread_mutex_unlock(&pt->pteList[i].scopeLock);
        
        Pthread_mutex_lock(&bk->loggingLock);
        printf("***** Stack unwinding done *****\n\n");
        Pthread_mutex_unlock(&bk->loggingLock);

        Pthread_mutex_unlock(&bk->compactLock);        
    }
    /* If running in garbage collector thread */
    else{
        Pthread_mutex_lock(&bk->compactLock);

        Pthread_mutex_lock(&bk->loggingLock);
        printf("***** Garbage collector will free up space in logical memory (sweep phase) *****\n\n");
        Pthread_mutex_unlock(&bk->loggingLock);
        
        for(int i = 0; i < pt->numEntries; i++){
            Pthread_mutex_lock(&pt->pteList[i].scopeLock);
            if(pt->pteList[i].scope == -1){
                pt->pteList[i].scope = -2;
                Pthread_mutex_unlock(&pt->pteList[i].scopeLock);

                freeElementMem(&(pt->pteList[i]), 1);
            }
            else{
                Pthread_mutex_unlock(&pt->pteList[i].scopeLock);
            }
        }

        Pthread_mutex_lock(&bk->loggingLock);
        printf("***** Sweep phase done *****\n\n");
        Pthread_mutex_unlock(&bk->loggingLock);

        Pthread_mutex_unlock(&bk->compactLock);
    }
}

void *gcRunner(void *param){
    while(1){
        Pthread_mutex_lock(&bk->loggingLock);
        printf("***** Garbage collection thread wakes up *****\n\n");
        Pthread_mutex_unlock(&bk->loggingLock);

        gcRun(1);
        compactMem();
        timespec ts = {0, GC_SLEEP_TIME};
        nanosleep(&ts, nullptr);
    }
}

void gcInit(){
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    void *param;
    pthread_create(&(bk->gcTid), &attr, gcRunner, param);
}

void diagnose(){
    Pthread_mutex_lock(&bk->loggingLock);
    timeval tv;
    gettimeofday(&tv, NULL);
    printf("+ Amt of free space left in memory : %d ; Timestamp : %lf ms\n\n", bk->freeMem, (tv.tv_sec - bk->st.tv_sec)*1000 + (double)(tv.tv_usec - bk->st.tv_usec)/1000);
    Pthread_mutex_unlock(&bk->loggingLock);
}

void gcStop(){
    pthread_join((bk->gcTid), nullptr);
}

void printPageTable(){
    printf("\n---\n");
    for(int i=0;i<bk->pageTable.numEntries;i++)
        cout << bk->pageTable.pteList[i];
}

bool cmp(const PageTableEntry *a, const PageTableEntry *b){
    return a->logicalAddr < b->logicalAddr;
}

void copyBlock(int sz, int logicalSource, int logicalDest)
{
    int sourceWordIdx = logicalSource/4;
    int sourceWordOffset = logicalSource%4;
    int destWordIdx = logicalDest/4;
    int destWordOffset = logicalDest%4;
    unsigned int sourceWord = bk->logicalMem[sourceWordIdx];
    unsigned int destWord = bk->logicalMem[destWordIdx];

    Pthread_mutex_lock(&bk->loggingLock);
    printf("^ Word alignment (during compaction) : Read data from old address in chunk of 4 bytes \n");
    Pthread_mutex_unlock(&bk->loggingLock);

    unsigned int allones = (1ll << 32) - 1;
    unsigned int sourceMask = (((1ll << (8 * sz)) - 1) << (8 * (4 - sourceWordOffset - sz))); 
    unsigned int destMask = (allones ^ (((1ll << (8 * sz)) - 1) << (8 * (4 - destWordOffset - sz))));
    sourceWord &= sourceMask;
    destWord &= destMask;
    if(sourceWordOffset < destWordOffset){
        sourceWord = (sourceWord >> (8*(destWordOffset - sourceWordOffset)));
    }
    else if(sourceWordOffset > destWordOffset){
        sourceWord = (sourceWord << (8*(sourceWordOffset - destWordOffset)));
    }
    unsigned int fourByteChunk = (sourceWord | destWord);
    bk->logicalMem[destWordIdx] = fourByteChunk;

    Pthread_mutex_lock(&bk->loggingLock);
    printf("^ Word alignment (during compaction) : Wrote data to new address in chunk of 4 bytes\n");
    Pthread_mutex_unlock(&bk->loggingLock);
}

void compactMem(){
    AVLTree<pair<int, int>> *fs = &(bk->freeSpaces);
    PageTable *pt = &(bk->pageTable);
    
    Pthread_mutex_lock(&(bk->compactLock));

    pair<int, int> temp2 = {-1,-1};
    fs->secondLast(&temp2);
    int secondLargest = temp2.first;
    pair<int,int> temp1 = fs->end()->data;
    int largest = temp1.first;
    int start = temp1.second;
    if(start + largest == bk->sizeOfMem){
        largest = secondLargest;
    }
    if(largest >= LARGE_HOLE_SIZE){
        Pthread_mutex_lock(&bk->loggingLock);
        printf("# Found a large hole of size : %d bytes\nCompacting...\n\n", largest);
        Pthread_mutex_unlock(&bk->loggingLock);

        PageTableEntry *ptCopy[pt->numEntries];
        int fldno  =0;
        for(int i=0; i < pt->numEntries; i++){
            if(pt->pteList[i].scope>=0)
                ptCopy[fldno++] = pt->pteList + i;
        }
        
        sort(ptCopy,ptCopy+fldno,cmp);

        for(int i=0; i < pt->numEntries; i++){
            Pthread_mutex_lock(&pt->pteList[i].scopeLock);
            if(pt->pteList[i].scope == -1){
                pt->pteList[i].scope = -2;
                Pthread_mutex_unlock(&pt->pteList[i].scopeLock);

                freeElementMem(&(pt->pteList[i]), 1);
            }
        }
        
        for(int i=0; i < fldno; i++){
            if(ptCopy[i]->scope >= 0){
                freeElementMem(ptCopy[i], 0);

                int spaceneeded = 0;
                int n = ptCopy[i]->numElements;
                int oldLogAddr = ptCopy[i]->logicalAddr;
                int size = typeSize[ptCopy[i]->type];

                switch(ptCopy[i]->type)
                {
                    case Int: 
                    case Char:
                        spaceneeded = n*size;
                        break;
                    case MediumInt:
                        if(n == 1){
                            spaceneeded = typeSize[ptCopy[i]->type];
                        }
                        else{
                            spaceneeded = n*typeSize[Int];
                        }
                        break;
                    case Boolean:
                        spaceneeded = ((n+7)/8);
                        break;
                }

                int newLogAddr = findBestFitFreeSpace(spaceneeded);
                bk->freeMem -= spaceneeded;
                
                int j=0;

                Pthread_mutex_lock(&ptCopy[i]->addrLock);
                if(ptCopy[i]->type == Boolean){
                    for(j = 0; j < (n / 32); j++){
                        copyBlock(4, oldLogAddr + j*4, newLogAddr + j*4);
                    }
                    if(n % 32){
                        copyBlock(1, oldLogAddr + j*4, newLogAddr + j*4);
                    }
                }
                else if(ptCopy[i]->type == MediumInt){
                    if(n > 1){
                        for(j = 0; j < n; j++){
                            copyBlock(4, oldLogAddr + j*4, newLogAddr + j*4);
                        }
                    }
                    else{
                        copyBlock(size, oldLogAddr + j*4, newLogAddr + j*4);
                    }
                }
                else{
                    for(j=0; j<(n*size)/4; j++){
                        copyBlock(4, oldLogAddr + j*4, newLogAddr + j*4);
                    }
                    if((n*size)%4){
                        copyBlock(size, oldLogAddr + j*4, newLogAddr + j*4);
                    }
                }

                ptCopy[i]->logicalAddr = newLogAddr;
                Pthread_mutex_unlock(&ptCopy[i]->addrLock);

                Pthread_mutex_lock(&bk->loggingLock);
                printf("- Page table entry changed for: \'%s\'\n", ptCopy[i]->name);
                printf("logical address : %d\n", newLogAddr);
                Pthread_mutex_unlock(&bk->loggingLock);
            }
        }
        printf("\n");

        Pthread_mutex_unlock(&(bk->compactLock));
    }
    else{
        Pthread_mutex_unlock(&(bk->compactLock));
        return;
    }
}
