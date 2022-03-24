#include "memlab.h"

BookkeepingMem *dataStructs;
void *logicalMem;
unsigned int localAddrCounter = 0;
int currScope = 0;
static unsigned int typeSize[4] = {4, 3, 1, 1};
pthread_t gcTid;

BookkeepingMem::BookkeepingMem(){
    pthread_mutexattr_t attr;
    pthread_mutex_init(&(this->compactLock), &attr);
    pthread_mutexattr_destroy(&attr);
}

BookkeepingMem::~BookkeepingMem(){
    pthread_mutex_destroy(&(this->compactLock));
}

PageTableEntry::PageTableEntry(){
    pthread_mutexattr_t attr;
    pthread_mutex_init(&(this->mutexLock), &attr);
    pthread_mutexattr_destroy(&attr);
}

PageTableEntry::~PageTableEntry(){
    pthread_mutex_destroy(&(this->mutexLock));
}

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

ostream &operator<<(ostream &op, const PageTableEntry &pte){
    op << pte.name << ", " << pte.scope << "\n";
    return op;
}

int BookkeepingMem::findBestFitFreeSpace(unsigned int size){
    /* Find the smallest free space greater than size in freeSpaces */
    // freeSpaces.inorder();
    // cout<<"\n";
    // freeIntervals.inorder();
    // cout<<"\n";
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
    dataStructs = new BookkeepingMem;
    logicalMem = malloc(memSize);
    dataStructs->freeSpaces.insert({memSize,0});
    dataStructs->freeIntervals.insert({0,memSize});
}

PageTableEntry *createVar(char varName[32], DataType type){
    PageTable *pt = &(dataStructs->pageTable);

    for(int ind = 0; ind<pt->numfields; ind++)
    {
        if(strcmp(pt->ptEntry[ind].name,varName)==0 && pt->ptEntry[ind].scope!=-1)
        {
            cout<<"Variable with the same name already exists\n";
            return NULL;
        }
    }

    pthread_mutex_lock(&(dataStructs->compactLock));
    int spaceAssignedInd = dataStructs->findBestFitFreeSpace(typeSize[type]);

    if(spaceAssignedInd == -1)
    {
        cout<<"No Space for Variable\n";
        pthread_mutex_unlock(&(dataStructs->compactLock));
        return NULL;
    }
    cout<<"Assigned "<<typeSize[type]<<" bytes of space at "<<spaceAssignedInd<<"\n";
    /* Add entry in page table */
    int idx = pt->numfields;
    strcpy(pt->ptEntry[idx].name,varName);
    pt->ptEntry[idx].type = type;
    pt->ptEntry[idx].localAddr = localAddrCounter;
    pt->ptEntry[idx].logicalAddr = spaceAssignedInd;
    pt->ptEntry[idx].scope = currScope;
    pt->ptEntry[idx].num_elements = 1;
    pt->numfields++;
    localAddrCounter +=4;
    pthread_mutex_unlock(&(dataStructs->compactLock));

    return &pt->ptEntry[idx];
}

void assignValueinMem(int logicalAddr, int size, DataType type, int value)
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

    // unsigned char* logicalMemChar = (unsigned char*)logicalMem;
    // int cpyval = (value<<(8*(4-size)));
    // memcpy(logicalMemChar + logicalAddr, &value , size);
    unsigned int* logicalMemInt = (unsigned int*)logicalMem;
    unsigned int fourByteChunk = logicalMemInt[logicalAddr/4];
    int startBit = 32-logicalAddr%4*8;
    int endBit = startBit - size*8;
    unsigned int mask = 0;
    mask+= ((1<<endBit)-1);
    mask+= ((1ll<<32ll)-1ll)-((1ll<<startBit)-1);
    unsigned int cpnum = value;
    cpnum = (cpnum<<(endBit));
    fourByteChunk&=mask;
    fourByteChunk|=cpnum;
    logicalMemInt[logicalAddr/4] = fourByteChunk;
}


void assignVar(PageTableEntry* ptr, int value)
{
    if(ptr==NULL)
    {
        cout<<"Invalid Pointer\n";
        return;
    }
    int logAdd = ptr->logicalAddr;
    int size = typeSize[ptr->type];
    assignValueinMem(logAdd,size,ptr->type,value);
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
    unsigned int* logicalMemInt = (unsigned int*)logicalMem;
    unsigned int fourByteChunk = logicalMemInt[logAdd/4];
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
    PageTable *pt = &(dataStructs->pageTable);

    for(int ind = 0; ind<pt->numfields; ind++)
    {
        if(strcmp(pt->ptEntry[ind].name,arrName)==0)
        {
            cout<<"Variable with the same name already exists\n";
            return NULL;
        }
    }
    int spaceneeded = 0;
    cout<<elements<<" yo "<<typeSize[type]<<"\n";
    switch(type)
    {
        case Int: 
        case Char:
            spaceneeded = elements*typeSize[type];
            break;
        case MediumInt:
            spaceneeded = elements*typeSize[Int];
            break;
        case Boolean:
            spaceneeded = ((elements+7)/8);
            break;
    }
    pthread_mutex_lock(&(dataStructs->compactLock));
    int spaceAssignedInd = dataStructs->findBestFitFreeSpace(spaceneeded);
    if(spaceAssignedInd == -1)
    {
        cout<<"No Space for Variable\n";
        pthread_mutex_unlock(&(dataStructs->compactLock));
        return NULL;
    }
    cout<<"Assigned "<<spaceneeded<<" bytes of space at "<<spaceAssignedInd<<"\n";
    int idx = pt->numfields;
    strcpy(pt->ptEntry[idx].name,arrName);
    pt->ptEntry[idx].type = type;
    pt->ptEntry[idx].localAddr = localAddrCounter;
    pt->ptEntry[idx].logicalAddr = spaceAssignedInd;
    pt->ptEntry[idx].scope = currScope;
    pt->ptEntry[idx].num_elements = elements;
    pt->numfields++;
    localAddrCounter +=4;
    pthread_mutex_unlock(&(dataStructs->compactLock));

    return &(pt->ptEntry[idx]);


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
        unsigned int* logicalMemChar = (unsigned int*)logicalMem;
        unsigned int byteChunk = logicalMemChar[FourByteChunkInd];
        int bitPos = 32 - ((ptr->logicalAddr)%4*8 + index);
        if(((byteChunk&(1<<bitPos))>>bitPos)!=value)
        {
            byteChunk = byteChunk^(1<<bitPos);
        }
        logicalMemChar[FourByteChunkInd] = byteChunk;

    }
    else 
    {
        assignValueinMem(ptr->logicalAddr+offset*index,typeSize[type],type,value);
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
        unsigned int* logicalMemChar = (unsigned int*)logicalMem;
        unsigned int byteChunk = logicalMemChar[FourByteChunkInd];
        int bitPos = 32 - ((ptr->logicalAddr)%4*8 + index);
        return (byteChunk&(1<<bitPos))>>bitPos;
    }
    else 
    {
        
        int retval = 0;
        int logAdd = ptr->logicalAddr+offset*index;
        int size = typeSize[ptr->type];
        unsigned int* logicalMemInt = (unsigned int*)logicalMem;
        unsigned int fourByteChunk = logicalMemInt[logAdd/4];
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
    if(ptr->num_elements == 1)
    {
        space = typeSize[ptr->type];
    }
    else 
    {
        switch(ptr->type)
        {
            case Int:
            case Char:
                space = typeSize[ptr->type] * ptr->num_elements ;
                break;
            case MediumInt:
                space = 4 * ptr->num_elements ;
                break;
            case Boolean:
                space = (ptr->num_elements+7)/8;
                break;
        }
    }

    // Find entry in both sets
    
    pair<int,int>intervalEntry = {logicalAddr,logicalAddr+space};
    pair<int,int>spaceEntry = {space,logicalAddr};
    int start = intervalEntry.first;
    int end = intervalEntry.second;

    // Check if free space starts right after
    Node<pair<int,int>>* intervalEntryBefore = dataStructs->freeIntervals.lower_bound({intervalEntry.second,-1});
    
    if(intervalEntryBefore!=NULL && intervalEntryBefore->data.first == intervalEntry.second)
    {
        pair<int,int>pp = intervalEntryBefore->data;
        dataStructs->freeIntervals.remove(pp);
        dataStructs->freeSpaces.remove({pp.second-pp.first,pp.first});
        end = pp.second;
    }
    //Check if free space ends right before using binary search
    int ans = -1;
    int low = 0;
    int high = start;
    while(low<=high)
    {
        int mid = (low+high)/2;
        Node<pair<int,int>>*p= dataStructs->freeIntervals.lower_bound({mid,-1});
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
    if(ans!=-1 && dataStructs->freeIntervals.lower_bound({ans,-1})->data.second == start )
    {
        start = ans;
        pair<int,int>p1 = dataStructs->freeIntervals.lower_bound({ans,-1})->data;
        dataStructs->freeIntervals.remove(p1);
        dataStructs->freeSpaces.remove({p1.second-p1.first,p1.first});
    }


    if(start%4==0)
    {
        if((end-start)%4==0)
        {
            pair<int,int>intervalEntry = {start,end};
            pair<int,int>spaceEntry = {end-start,start};
            // Check if free space starts right after
            Node<pair<int,int>>* intervalEntryBefore = dataStructs->freeIntervals.lower_bound({intervalEntry.second,-1});
            
            if(intervalEntryBefore!=NULL && intervalEntryBefore->data.first == intervalEntry.second)
            {
                pair<int,int>pp = intervalEntryBefore->data;
                dataStructs->freeIntervals.remove(pp);
                dataStructs->freeSpaces.remove({pp.second-pp.first,pp.first});
                end = pp.second;
            }
            //Check if free space ends right before using binary search
            int ans = -1;
            int low = 0;
            int high = start;
            while(low<=high)
            {
                int mid = (low+high)/2;
                Node<pair<int,int>>*p= dataStructs->freeIntervals.lower_bound({mid,-1});
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
            if(ans!=-1 && dataStructs->freeIntervals.lower_bound({ans,-1})->data.second == start )
            {
                start = ans;
                pair<int,int>p1 = dataStructs->freeIntervals.lower_bound({ans,-1})->data;
                dataStructs->freeIntervals.remove(p1);
                dataStructs->freeSpaces.remove({p1.second-p1.first,p1.first});
            }
            dataStructs->freeSpaces.insert({end-start,start});
            dataStructs->freeIntervals.insert({start,end});
        }
        else 
        {
            dataStructs->freeSpaces.insert({end-start,start});
            dataStructs->freeIntervals.insert({start,end});
        }
    }
    else 
    {
        int extra = 4-start%4;
        dataStructs->freeIntervals.insert({start+extra,end});
        dataStructs->freeSpaces.insert({end-start-extra,start+extra});
        dataStructs->freeIntervals.insert({start,start+extra});
        dataStructs->freeSpaces.insert({extra,start});
        

    }

    //dataStructs->freeIntervals.inorder();
    if(destroy){
        ptr->scope = -2;
    }
    cout<<"Freed up Space between "<<intervalEntry.first<<" and "<<intervalEntry.second<<"\n";
    cout<<"Free Space Segment Created Between "<<start<<" and "<<end<<"\n";

}

void freeElement(PageTableEntry *pte){
    /* Mark */
    pte->scope = -1;
}

void startScope(){
    currScope++;
}

void endScope(){
    PageTable *pt = &(dataStructs->pageTable);
    for(int i = 0; i < pt->numfields; i++){
        if(pt->ptEntry[i].scope == currScope)
            freeElement(&(pt->ptEntry[i]));
    }
    currScope--;
}

void gcInit(){
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    void *param;
    pthread_create(&gcTid, &attr, gcRunner, param);
}

void gcStop(){
    pthread_join(gcTid, NULL);
}

void gcRun(int opt){
    PageTable *pt = &(dataStructs->pageTable);
    int i = pt->numfields - 1;
    /* If running in main thread */
    if(opt == 0){
        if(i >= 0)
            pthread_mutex_lock(&pt->ptEntry[i].mutexLock);
        while(i >= 0 && pt->ptEntry[i].scope == -1){
            pt->ptEntry[i].scope = -2;
            pthread_mutex_unlock(&pt->ptEntry[i].mutexLock);
            
            freeElementMem(&(pt->ptEntry[i]), 1);
            
            pt->numfields--;
            
            i--;
            if(i >= 0)
                pthread_mutex_lock(&pt->ptEntry[i].mutexLock);
        }
        if(i >= 0)
            pthread_mutex_unlock(&pt->ptEntry[i].mutexLock);
    }
    /* If running in garbage collector thread */
    else{
        for(int i = 0; i < pt->numfields; i++){
            pthread_mutex_lock(&pt->ptEntry[i].mutexLock);
            if(pt->ptEntry[i].scope == -1){
                pt->ptEntry[i].scope = -2;
                pthread_mutex_unlock(&pt->ptEntry[i].mutexLock);

                freeElementMem(&(pt->ptEntry[i]), 1);
            }
            else{
                pthread_mutex_unlock(&pt->ptEntry[i].mutexLock);
            }
        }
    }
}

void *gcRunner(void *param){
    while(1){
        gcRun(1);
        compactMem();
        timespec ts = {0, 100000000};
        nanosleep(&ts, NULL);
    }
}

void printPageTable(){
    cout<<"\n-----\n";
    for(int i=0;i<dataStructs->pageTable.numfields;i++)
        cout << dataStructs->pageTable.ptEntry[i];
    cout<<"-----\n";
}

void compactMem(){
    AVLTree<pair<int, int>> *fs = &(dataStructs->freeSpaces);
    PageTable *pt = &(dataStructs->pageTable);
    pthread_mutex_lock(&(dataStructs->compactLock));
    if(fs->end()->data.first >= LARGE_HOLE_SIZE){
        for(int i=0; i < pt->numfields; i++){
            if(pt->ptEntry[i].scope >= 0){
                freeElementMem(&(pt->ptEntry[i]), 0);
            }
            else if(pt->ptEntry[i].scope == -1){
                freeElementMem(&(pt->ptEntry[i]), 1);
            }
        }
        for(int i=0; i < pt->numfields; i++){
            if(pt->ptEntry[i].scope >= 0){
                int logAddr = dataStructs->findBestFitFreeSpace(pt->ptEntry[i].num_elements * typeSize[pt->ptEntry[i].type]);
                pt->ptEntry[i].logicalAddr = logAddr;
            }
        }
        pthread_mutex_unlock(&(dataStructs->compactLock));
    }
    else{
        pthread_mutex_unlock(&(dataStructs->compactLock));
        return;
    }
}

/*
Best fit logic:
1. var
- Find smallest free space greater than dataType.size : fslb = freeSpaces.lower_bound({dataType.size, -1})
- Find the corresponding interval in freeIntervals : filb = freeIntervals.lower_bound({fslb.start, -1})
- Data is put into the free space. Copy fslb to remWord. Copy filb to remInterval. Delete fslb. Delete filb
- restSpace.second = remWord.second + 4; restSpace.space = remWord.space - 4;
- remWord.second += dataType.size; remWord.space = 4 - dataType.size
- remInterval.start = remWord.second; remInterval.end = remInterval.start + remWord.space - 1
- restInterval.start = restSpace.second; restInterval.end = restInterval.start + restSpace.space
- Add remWord and restSpace to freeSpaces. Add remInterval and restInterval to freeIntervals

Free up logic:
1. var
- var.logicalAddr is the logical address of this variable
- Search for {var.logicalAddr + var.size, ?}, lets say the size is nextsz
- if (var.logicalAdd + var.size) % 4 == 0
-- 
- else
-- 
*/