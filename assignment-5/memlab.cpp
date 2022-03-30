/* TODO
 * L mutex lock for print statements
 * P localAddress increment scheme change?
 * P definition of large hole based on size of allocated memory?
 * P error handling : a. invalid type passed, b. referring to variable that has been marked or deleted
 * P when copyblocking mediumints, the 4th byte also gets copied, to do or not?
 * P change reallocating policy during compactmem to largest first?
 * logging lock nahi chal raha
 */
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
    pthread_mutex_init(&(this->mutexLock), &attr);
    pthread_mutexattr_destroy(&attr);
}

/* PageTableEntry destructor */
PageTableEntry::~PageTableEntry(){
    pthread_mutex_destroy(&(this->mutexLock));
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

int BookkeepingMem::findBestFitFreeSpace(unsigned int size){
    /* Find the smallest free space greater than size in freeSpaces */
    Node<pair<int,int>>* _fslb = freeSpaces.lower_bound({size, -1});
    if(_fslb == nullptr)
    {
        return -1;
    }
   
    pair<int, int> fslb = _fslb->data;

    /* Find the corresponding interval in freeIntervals */
    pair<int, int> filb = freeIntervals.lower_bound({fslb.second, -1})->data;


    pair<int, int> remWord = fslb;
    pair<int, int> remInterval = filb;

    /* Delete the existing intervals */
    freeIntervals.remove(filb);
    freeSpaces.remove(fslb);

    int totspace = fslb.first;
    int modspace = (totspace-size)%4;

    /* Inserting right half of remaining chunk */
    if(totspace-size-modspace !=0)
    {
        freeSpaces.insert({totspace-size-modspace,remWord.second+size+modspace});
        freeIntervals.insert({remWord.second+size+modspace,remWord.second+totspace});
    }

    /* Inserting left half of remaining chunk  */
    if(modspace!=0)
    {
        freeSpaces.insert({modspace,remWord.second+size});
        freeIntervals.insert({remWord.second+size,remWord.second+size+modspace});
    }

    return fslb.second;
}

void createMem(int memSize) {
    bk = new BookkeepingMem;
    bk->sizeOfMem = memSize;
    bk->freeMem = memSize;
    cout<<"- Allocated space for bookkeeping data structures\n";
    bk->freeSpaces.insert({memSize,0});
    bk->freeIntervals.insert({0,memSize});
    cout<<"- Initialized data structures\n";
    bk->logicalMem = (unsigned int *)malloc(memSize);
    cout<<"- Allocated space for required by user\n";
    cout<<"\n";
    gettimeofday(&bk->st, NULL);
}

PageTableEntry *createVar(char varName[32], DataType type){
    PageTable *pt = &(bk->pageTable);

    for(int ind = 0; ind<pt->numEntries; ind++)
    {
        if(strcmp(pt->pteList[ind].name,varName) == 0 && pt->pteList[ind].scope >= 0)
        {
            Pthread_mutex_lock(&bk->loggingLock);
            cout<<"Variable with name \'"<<varName<<"\' already exists\n";
            Pthread_mutex_unlock(&bk->loggingLock);
            return nullptr;
        }
    }

    Pthread_mutex_lock(&(bk->compactLock));

    int spaceAssignedInd = bk->findBestFitFreeSpace(typeSize[type]);
    if(spaceAssignedInd == -1)
    {
        Pthread_mutex_lock(&bk->loggingLock);
        cout<<"No Space for Variable\n";
        Pthread_mutex_unlock(&bk->loggingLock);

        Pthread_mutex_unlock(&(bk->compactLock));
        return nullptr;
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
    pt->numEntries++;
    bk->localAddrCounter +=4;

    Pthread_mutex_lock(&bk->loggingLock);
    cout<<"- Page table entry created for variable: \'"<<varName<<"\'\n";
    cout<<"type : "<<typeName[type]<<"\n";
    cout<<"local address : "<<bk->localAddrCounter - 4<<"\n";
    cout<<"logical address : "<<spaceAssignedInd<<"\n\n";
    Pthread_mutex_unlock(&bk->loggingLock);

    Pthread_mutex_unlock(&(bk->compactLock));

    return &pt->pteList[idx];
}

void assignValueInMem(int logicalAddr, int size, DataType type, int value)
{
    if((type == Boolean && (value !=0 && value!=1)))
    {
        Pthread_mutex_lock(&bk->loggingLock);
        cout<<"Value Being Assigned Cannot Fit In DataType\n";
        Pthread_mutex_unlock(&bk->loggingLock);
        return;
    }
    if(type != Boolean && (value >=(1ll<<(8*typeSize[type]-1))|| value < -(1ll<<(8*typeSize[type]-1))))
    {
        Pthread_mutex_lock(&bk->loggingLock);
        cout<<"Value Being Assigned Cannot Fit In DataType\n";
        Pthread_mutex_unlock(&bk->loggingLock);
        return;
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
}


void assignVar(PageTableEntry* ptr, int value)
{
    if(ptr == nullptr)
    {
        Pthread_mutex_lock(&bk->loggingLock);
        cout<<"Invalid Pointer\n";
        Pthread_mutex_unlock(&bk->loggingLock);
        return;
    }
    
    int logAdd = ptr->logicalAddr;
    int size = typeSize[ptr->type];
    assignValueInMem(logAdd,size,ptr->type,value);
    Pthread_mutex_lock(&bk->loggingLock);
    cout<<"- Assign value to variable\n";
    cout<<ptr->name<<" := "<<value<<"\n\n";
    Pthread_mutex_unlock(&bk->loggingLock);
}

int varValue(PageTableEntry* ptr)
{
    if(ptr==nullptr)
    {
        cout<<"Invalid Pointer\n";
        return -1;
    }
    int retval = 0;
    int logAdd = ptr->logicalAddr;
    int size = typeSize[ptr->type];
    unsigned int fourByteChunk = bk->logicalMem[logAdd/4];
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
    return retval;
}

PageTableEntry* createArr(char arrName[32], DataType type, int elements){
    PageTable *pt = &(bk->pageTable);

    for(int ind = 0; ind<pt->numEntries; ind++)
    {
        if(strcmp(pt->pteList[ind].name,arrName)==0)
        {
            Pthread_mutex_lock(&bk->loggingLock);
            cout<<"Variable with name \'"<<arrName<<"\' already exists\n";
            Pthread_mutex_unlock(&bk->loggingLock);
            return nullptr;
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
                cout<<"- Word alignment : MediumInt array elements are stored at offsets of 4\nSo space needed for array is 4 * (numElements)\n";
                Pthread_mutex_unlock(&bk->loggingLock);
                spaceneeded = elements*typeSize[Int];
            }
            break;
        case Boolean:
            spaceneeded = ((elements+7)/8);
            break;
    }
    Pthread_mutex_lock(&(bk->compactLock));
    int spaceAssignedInd = bk->findBestFitFreeSpace(spaceneeded);
    if(spaceAssignedInd == -1)
    {   
        Pthread_mutex_lock(&bk->loggingLock);
        cout<<"No Space for Array\n";
        Pthread_mutex_unlock(&bk->loggingLock);

        Pthread_mutex_unlock(&(bk->compactLock));
        return nullptr;
    }
    bk->freeMem -= spaceneeded;
    Pthread_mutex_lock(&bk->loggingLock);
    cout<<"- \'"<<arrName<<"\' array assigned "<<spaceneeded<<" bytes (for "<<elements<<" elements) in memory\n";
    Pthread_mutex_unlock(&bk->loggingLock);

    int idx = pt->numEntries;
    strcpy(pt->pteList[idx].name,arrName);
    pt->pteList[idx].type = type;
    pt->pteList[idx].localAddr = bk->localAddrCounter;
    pt->pteList[idx].logicalAddr = spaceAssignedInd;
    pt->pteList[idx].scope = bk->currScope;
    pt->pteList[idx].numElements = elements;
    pt->numEntries++;
    bk->localAddrCounter +=4;

    Pthread_mutex_lock(&bk->loggingLock);
    cout<<"- Page table entry created for array: \'"<<arrName<<"\'\n";
    cout<<"type : "<<typeName[type]<<"\n";
    cout<<"local address : "<<bk->localAddrCounter - 4<<"\n";
    cout<<"logical address : "<<spaceAssignedInd<<"\n";
    cout<<"number of elements in array : "<<elements<<"\n\n";
    Pthread_mutex_unlock(&bk->loggingLock);

    Pthread_mutex_unlock(&(bk->compactLock));

    return &(pt->pteList[idx]);
}

void assignArr(PageTableEntry* ptr, int index, int value)
{
    if(ptr==nullptr)
    {
        Pthread_mutex_lock(&bk->loggingLock);
        cout<<"Invalid Pointer\n";
        Pthread_mutex_unlock(&bk->loggingLock);
        return;
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
            cout<<"Value Being Assigned Cannot Fit In DataType\n";
            Pthread_mutex_unlock(&bk->loggingLock);
            return;
        }
        int byteChunkInd = ptr->logicalAddr +  index/8;
        int FourByteChunkInd = (byteChunkInd/4);
        unsigned int* logicalMemChar = (unsigned int*)bk->logicalMem;
        unsigned int byteChunk = logicalMemChar[FourByteChunkInd];
        int bitPos = 32 - ((ptr->logicalAddr)%4*8 + index);
        if(((byteChunk&(1<<bitPos))>>bitPos)!=value)
        {
            byteChunk = byteChunk^(1<<bitPos);
        }
        logicalMemChar[FourByteChunkInd] = byteChunk;
        Pthread_mutex_lock(&bk->loggingLock);
        cout<<"- Assign value at array index\n";
        cout<<ptr->name<<"["<<index<<"] := "<<value<<"\n\n";
        Pthread_mutex_unlock(&bk->loggingLock);
    }
    else 
    {
        assignValueInMem(ptr->logicalAddr+offset*index,typeSize[type],type,value);
        Pthread_mutex_lock(&bk->loggingLock);
        cout<<"- Assign value at array index\n";
        cout<<ptr->name<<"["<<index<<"] := "<<value<<"\n\n";
        Pthread_mutex_unlock(&bk->loggingLock);
    }
}

int arrValue(PageTableEntry* ptr, int index)
{
    if(ptr==nullptr)
    {
        Pthread_mutex_lock(&bk->loggingLock);   
        cout<<"Invalid Pointer\n";
        Pthread_mutex_unlock(&bk->loggingLock);
        
        return -1;
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
        int byteChunkInd = ptr->logicalAddr +  index/8;
        int FourByteChunkInd = (byteChunkInd/4);
        unsigned int* logicalMemChar = (unsigned int*)bk->logicalMem;
        unsigned int byteChunk = logicalMemChar[FourByteChunkInd];
        int bitPos = 32 - ((ptr->logicalAddr)%4*8 + index);
        return (byteChunk&(1<<bitPos))>>bitPos;
    }
    else 
    {
        
        int retval = 0;
        int logAdd = ptr->logicalAddr+offset*index;
        int size = typeSize[ptr->type];
        unsigned int fourByteChunk = bk->logicalMem[logAdd/4];
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
        return retval;
    }
}

void freeElementMem(PageTableEntry* ptr, int destroy)
{
    if(ptr==nullptr)
    {
        Pthread_mutex_lock(&bk->loggingLock);
        cout<<"Invalid Pointer\n";
        Pthread_mutex_unlock(&bk->loggingLock);
        return;
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
                if(ptr->numElements == 1){
                    space = typeSize[ptr->type];
                }
                else{
                    space = typeSize[Int] * ptr->numElements;
                }
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


    if(start%4==0)
    {
        if((end-start)%4==0)
        {
            pair<int,int>intervalEntry = {start,end};
            pair<int,int>spaceEntry = {end-start,start};
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
            bk->freeSpaces.insert({end-start,start});
            bk->freeIntervals.insert({start,end});
        }
        else 
        {
            bk->freeSpaces.insert({end-start,start});
            bk->freeIntervals.insert({start,end});
        }
    }
    else 
    {
        int extra = 4-start%4;
        bk->freeIntervals.insert({start+extra,end});
        bk->freeSpaces.insert({end-start-extra,start+extra});
        bk->freeIntervals.insert({start,start+extra});
        bk->freeSpaces.insert({extra,start});
        

    }

    bk->freeMem += space;
    //bk->freeIntervals.inorder();
    if(destroy){
        Pthread_mutex_lock(&bk->loggingLock);
        cout<<"- Page table entry deleted (memory freed) for: \'"<<ptr->name<<"\'\n\n";
        Pthread_mutex_unlock(&bk->loggingLock);
        ptr->scope = -2;
    }
    Pthread_mutex_lock(&bk->loggingLock);
    cout<<"Freed up Space between "<<intervalEntry.first<<" and "<<intervalEntry.second<<"\n";
    cout<<"Free Space Segment Created Between "<<start<<" and "<<end<<"\n\n";
    // diagnose();
    Pthread_mutex_unlock(&bk->loggingLock);

}

void freeElement(PageTableEntry *pte){
    /* Mark */
    cout<<"- Page table entry marked for deletion for: \'"<<pte->name<<"\'\n\n";
    pte->scope = -1;
}

void startScope(){
    bk->currScope++;
}

void endScope(){
    PageTable *pt = &(bk->pageTable);
    for(int i = pt->numEntries - 1; i >= 0; i--){
        if(pt->pteList[i].scope == bk->currScope)
            freeElement(&(pt->pteList[i]));
    }
    bk->currScope--;
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
    printf("+ Amt of free space left in memory : %d ; Timestamp : %lf ms", bk->freeMem, (tv.tv_sec - bk->st.tv_sec)*1000 + (double)(tv.tv_usec - bk->st.tv_usec)/1000);
    Pthread_mutex_unlock(&bk->loggingLock);
}

void gcStop(){
    pthread_join((bk->gcTid), nullptr);
}

void gcRun(int opt){
    PageTable *pt = &(bk->pageTable);
    int i = pt->numEntries - 1;
    /* If running in main thread */
    if(opt == 0){
        Pthread_mutex_lock(&bk->loggingLock);
        cout<<"***** Garbage collector will unwind the stack and free up corresponding memory *****\n\n";
        Pthread_mutex_unlock(&bk->loggingLock);

        if(i >= 0)
            Pthread_mutex_lock(&pt->pteList[i].mutexLock);
        while(i >= 0 && pt->pteList[i].scope < 0){
            if(pt->pteList[i].scope == -1){
                Pthread_mutex_unlock(&pt->pteList[i].mutexLock);

                Pthread_mutex_lock(&bk->compactLock);
                freeElementMem(&(pt->pteList[i]), 1);
                Pthread_mutex_unlock(&bk->compactLock);
            }
            else{
                Pthread_mutex_unlock(&pt->pteList[i].mutexLock);
            }

            pt->numEntries--;

            i--;
            if(i >= 0)
                Pthread_mutex_lock(&pt->pteList[i].mutexLock);
        }
        if(i >= 0)
            Pthread_mutex_unlock(&pt->pteList[i].mutexLock);
        
        Pthread_mutex_lock(&bk->loggingLock);
        cout<<"***** Stack unwinding done *****\n\n";
        Pthread_mutex_unlock(&bk->loggingLock);
    }
    /* If running in garbage collector thread */
    else{
        Pthread_mutex_lock(&bk->loggingLock);
        cout<<"***** Garbage collector will free up space in logical memory (sweep phase) *****\n\n";
        Pthread_mutex_unlock(&bk->loggingLock);
        
        for(int i = 0; i < pt->numEntries; i++){
            Pthread_mutex_lock(&pt->pteList[i].mutexLock);
            if(pt->pteList[i].scope == -1){
                Pthread_mutex_unlock(&pt->pteList[i].mutexLock);

                Pthread_mutex_lock(&bk->compactLock);
                freeElementMem(&(pt->pteList[i]), 1);
                Pthread_mutex_unlock(&bk->compactLock);
            }
            else{
                Pthread_mutex_unlock(&pt->pteList[i].mutexLock);
            }
        }

        Pthread_mutex_lock(&bk->loggingLock);
        cout<<"***** Sweep phase done *****\n\n";
        Pthread_mutex_unlock(&bk->loggingLock);
    }
}

void *gcRunner(void *param){
    while(1){
        Pthread_mutex_lock(&bk->loggingLock);
        cout<<"***** Garbage collection thread wakes up *****\n\n";
        Pthread_mutex_unlock(&bk->loggingLock);

        gcRun(1);
        compactMem();
        timespec ts = {0, 10000000};
        nanosleep(&ts, nullptr);
    }
}

void printPageTable(){
    cout<<"\n---\n";
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
    printf("Here you go : %d\n", largest);
    if(largest >= LARGE_HOLE_SIZE){
        Pthread_mutex_lock(&bk->loggingLock);
        cout<<"Found a large hole of size : "<<largest<<" bytes\nCompacting...\n\n";
        Pthread_mutex_unlock(&bk->loggingLock);

        PageTableEntry *ptCopy[pt->numEntries];
        int fldno  =0;
        for(int i=0; i < pt->numEntries; i++){
            if(pt->pteList[i].scope>=0)
                ptCopy[fldno++] = pt->pteList + i;
        }
        
        sort(ptCopy,ptCopy+fldno,cmp);

        for(int i=0; i < pt->numEntries; i++){
            if(pt->pteList[i].scope >= 0){
                freeElementMem(&(pt->pteList[i]), 0);
            }
            else if(pt->pteList[i].scope == -1){
                freeElementMem(&(pt->pteList[i]), 1);
            }
        }
        
        for(int i=0; i < fldno; i++){
            if(ptCopy[i]->scope >= 0){
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

                int newLogAddr = bk->findBestFitFreeSpace(spaceneeded);
                bk->freeMem -= spaceneeded;
                
                int j=0;
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
                Pthread_mutex_lock(&bk->loggingLock);
                cout<<"- Page table entry changed for: \'"<<ptCopy[i]->name<<"\'\n";
                cout<<"logical address : "<<newLogAddr<<"\n";
                // diagnose();
                Pthread_mutex_unlock(&bk->loggingLock);
            }
        }
        cout<<"\n";

        Pthread_mutex_unlock(&(bk->compactLock));
    }
    else{
        Pthread_mutex_unlock(&(bk->compactLock));
        return;
    }
}