/* TODO
 * 1. mutex lock for print statements
 * 2. compactmem separate handling for boolean etc
 * 3. localAddress increment scheme change?
 * 4. definition of large hole based on size of allocated memory?
 */
#include "memlab.h"

BookkeepingMem *bk; /* Bookkeeping data structures */

static unsigned int typeSize[4] = {4, 3, 1, 1};
static unsigned char typeName[4][10] = {"Int", "MediumInt", "Char", "Boolean"};

/* BookkeepingMem constructor */
BookkeepingMem::BookkeepingMem(){
    pthread_mutexattr_t attr;
    /* Initialise mutex lock for compaction */
    pthread_mutex_init(&(this->compactLock), &attr);
    pthread_mutexattr_destroy(&attr);
}

/* BookkeepingMem destructor */
BookkeepingMem::~BookkeepingMem(){
    pthread_mutex_destroy(&(this->compactLock));
}

/* PageTableEntry constructor */
PageTableEntry::PageTableEntry(){
    pthread_mutexattr_t attr;
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
    if(_fslb == NULL)
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
    cout<<"- Allocated space for bookkeeping data structures\n";
    bk->freeSpaces.insert({memSize,0});
    bk->freeIntervals.insert({0,memSize});
    cout<<"- Initialized data structures\n";
    bk->logicalMem = (unsigned int *)malloc(memSize);
    cout<<"- Allocated space for required by user\n";
    cout<<"\n";
}

PageTableEntry *createVar(char varName[32], DataType type){
    PageTable *pt = &(bk->pageTable);

    for(int ind = 0; ind<pt->numEntries; ind++)
    {
        if(strcmp(pt->pteList[ind].name,varName) == 0 && pt->pteList[ind].scope >= 0)
        {
            cout<<"Variable with name \'"<<varName<<"\' already exists\n";
            return NULL;
        }
    }

    pthread_mutex_lock(&(bk->compactLock));

    int spaceAssignedInd = bk->findBestFitFreeSpace(typeSize[type]);
    if(spaceAssignedInd == -1)
    {
        cout<<"No Space for Variable\n";
        pthread_mutex_unlock(&(bk->compactLock));
        return NULL;
    }
    cout<<"- \'"<<varName<<"\' assigned "<<typeSize[type]<<" bytes in memory\n";
    
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
    cout<<"- Page table entry created for variable: \'"<<varName<<"\'\n";
    cout<<"type : "<<typeName[type]<<"\n";
    cout<<"local address : "<<bk->localAddrCounter - 4<<"\n";
    cout<<"logical address : "<<spaceAssignedInd<<"\n\n";
    pthread_mutex_unlock(&(bk->compactLock));

    return &pt->pteList[idx];
}

void assignValueInMem(int logicalAddr, int size, DataType type, int value)
{
    if((type == Boolean && (value !=0 && value!=1)))
    {
        cout<<"Value Being Assigned Cannot Fit In DataType\n";
        return;
    }
    if(type != Boolean && (value >=(1ll<<(8*typeSize[type]-1))|| value < -(1ll<<(8*typeSize[type]-1))))
    {
        cout<<"Value Being Assigned Cannot Fit In DataType\n";
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
    if(ptr == NULL)
    {
        cout<<"Invalid Pointer\n";
        return;
    }
    int logAdd = ptr->logicalAddr;
    int size = typeSize[ptr->type];
    assignValueInMem(logAdd,size,ptr->type,value);
    cout<<"- Assign value to variable\n";
    cout<<ptr->name<<" := "<<value<<"\n\n";
}

int varValue(PageTableEntry* ptr)
{
    if(ptr==NULL)
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
            cout<<"Variable with name \'"<<arrName<<"\' already exists\n";
            return NULL;
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
            cout<<"- Word alignment : MediumInt array elements are stored at offsets of 4\nSo space needed for array is 4 * (numElements)\n\n";
            spaceneeded = elements*typeSize[Int];
            break;
        case Boolean:
            spaceneeded = ((elements+7)/8);
            break;
    }
    pthread_mutex_lock(&(bk->compactLock));
    int spaceAssignedInd = bk->findBestFitFreeSpace(spaceneeded);
    if(spaceAssignedInd == -1)
    {
        cout<<"No Space for Array\n";
        pthread_mutex_unlock(&(bk->compactLock));
        return NULL;
    }
    cout<<"- \'"<<arrName<<"\' array assigned ( "<<typeSize[type]<<" * "<<elements<<" = ) "<<spaceneeded<<" bytes in memory\n";
    int idx = pt->numEntries;
    strcpy(pt->pteList[idx].name,arrName);
    pt->pteList[idx].type = type;
    pt->pteList[idx].localAddr = bk->localAddrCounter;
    pt->pteList[idx].logicalAddr = spaceAssignedInd;
    pt->pteList[idx].scope = bk->currScope;
    pt->pteList[idx].numElements = elements;
    pt->numEntries++;
    bk->localAddrCounter +=4;
    cout<<"- Page table entry created for array: \'"<<arrName<<"\'\n";
    cout<<"type : "<<typeName[type]<<"\n";
    cout<<"local address : "<<bk->localAddrCounter - 4<<"\n";
    cout<<"logical address : "<<spaceAssignedInd<<"\n";
    cout<<"number of elements in array : "<<elements<<"\n\n";
    pthread_mutex_unlock(&(bk->compactLock));

    return &(pt->pteList[idx]);
}

void assignArr(PageTableEntry* ptr, int index, int value)
{
    if(ptr==NULL)
    {
        cout<<"Invalid Pointer\n";
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
            cout<<"Value Being Assigned Cannot Fit In DataType\n";
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
        cout<<"- Assign value at array index\n";
        cout<<ptr->name<<"["<<index<<"] := "<<value<<"\n\n";
    }
    else 
    {
        assignValueInMem(ptr->logicalAddr+offset*index,typeSize[type],type,value);
        cout<<"- Assign value at array index\n";
        cout<<ptr->name<<"["<<index<<"] := "<<value<<"\n\n";
    }
}

int arrValue(PageTableEntry* ptr, int index)
{
    if(ptr==NULL)
    {
        cout<<"Invalid Pointer\n";
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
    if(ptr==NULL)
    {
        cout<<"Invalid Pointer\n";
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
                space = 4 * ptr->numElements ;
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
    
    if(intervalEntryBefore!=NULL && intervalEntryBefore->data.first == intervalEntry.second)
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
        if(p==NULL || p->data.first >= start)
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
            
            if(intervalEntryBefore!=NULL && intervalEntryBefore->data.first == intervalEntry.second)
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
                if(p==NULL || p->data.first >= start)
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

    //bk->freeIntervals.inorder();
    if(destroy){
        cout<<"- Page table entry deleted (memory freed) for: \'"<<ptr->name<<"\'\n\n";
        ptr->scope = -2;
    }
    cout<<"Freed up Space between "<<intervalEntry.first<<" and "<<intervalEntry.second<<"\n";
    cout<<"Free Space Segment Created Between "<<start<<" and "<<end<<"\n\n";

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

void gcStop(){
    pthread_join((bk->gcTid), NULL);
}

void gcRun(int opt){
    PageTable *pt = &(bk->pageTable);
    int i = pt->numEntries - 1;
    /* If running in main thread */
    if(opt == 0){
        if(i >= 0)
            pthread_mutex_lock(&pt->pteList[i].mutexLock);
        while(i >= 0 && pt->pteList[i].scope < 0){
            if(pt->pteList[i].scope == -1){
                pthread_mutex_unlock(&pt->pteList[i].mutexLock);
                freeElementMem(&(pt->pteList[i]), 1);
            }
            else{
                pthread_mutex_unlock(&pt->pteList[i].mutexLock);
            }

            pt->numEntries--;

            i--;
            if(i >= 0)
                pthread_mutex_lock(&pt->pteList[i].mutexLock);
        }
        if(i >= 0)
            pthread_mutex_unlock(&pt->pteList[i].mutexLock);
    }
    /* If running in garbage collector thread */
    else{
        for(int i = 0; i < pt->numEntries; i++){
            pthread_mutex_lock(&pt->pteList[i].mutexLock);
            if(pt->pteList[i].scope == -1){
                pthread_mutex_unlock(&pt->pteList[i].mutexLock);

                freeElementMem(&(pt->pteList[i]), 1);
            }
            else{
                pthread_mutex_unlock(&pt->pteList[i].mutexLock);
            }
        }
    }
}

void *gcRunner(void *param){
    while(1){
        gcRun(1);
        compactMem();
        timespec ts = {0, 10000000};
        nanosleep(&ts, NULL);
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
    unsigned int sourceMask = (((1ll << (8 * sz)) - 1));
    unsigned int destMask = (allones ^ (((1ll << (8 * sz)) - 1) << (4 - destWordOffset - sz)));
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
    
    pthread_mutex_lock(&(bk->compactLock));

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
        
        cout<<"Found a large hole of size : "<<largest<<" bytes\nCompacting...\n\n";

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
                int newLogAddr = bk->findBestFitFreeSpace(ptCopy[i]->numElements * typeSize[ptCopy[i]->type]);
                
                int n = ptCopy[i]->numElements;
                int oldLogAddr = ptCopy[i]->logicalAddr;
                int size = typeSize[ptCopy[i]->type];
                
                int j=0;
                for(j=0; j<(n*size)/4; j++){
                    copyBlock(4, oldLogAddr + j*4, newLogAddr + j*4);
                }
                if(size < 4){
                    copyBlock(size, oldLogAddr + j*4, newLogAddr + j*4);
                }
                ptCopy[i]->logicalAddr = newLogAddr;
                
                cout<<"- Page table entry changed for: \'"<<ptCopy[i]->name<<"\'\n";
                cout<<"logical address : "<<newLogAddr<<"\n";
            }
        }
        cout<<"\n";

        pthread_mutex_unlock(&(bk->compactLock));
    }
    else{
        pthread_mutex_unlock(&(bk->compactLock));
        return;
    }
}