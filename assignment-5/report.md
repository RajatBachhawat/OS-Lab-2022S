### Structure of Page Table (Symbol Table)
```
class PageTable {
public:
	/* Number of entries in page table right now */
	int numEntries = 0;
	/* The page table */
	PageTableEntry pteList[PAGE_TABLE_SIZE];
};
```
- Our page table (or symbol table) is an array of `PageTableEntry` elements
- `numEntries` keeps track of the number of entries in the page table right now
- We have one and only one page table which stores the symbols created in all functions/scopes
- Deleting elements of the most recently ended scope is just as simple as decrementing `numEntries` by 1

Here's what a `PageTableEntry` object looks like:
```
class PageTableEntry {
public:
    /* Constructor */
    PageTableEntry();
    /* Destructor */
    ~PageTableEntry();
    /* Copy assignment operator */
    PageTableEntry &operator=(const PageTableEntry &other);
    
    /* Name of the variable / array */
    char name[MAX_NAME_SIZE];
    /* Type of the variable / array */
    DataType type;
    /* Local address of variable */
    int localAddr;
    /* Logical address of variable */
    int logicalAddr;
    /* Scope of the variable (controlled by global curr_scope) */
    int scope;
    /* Number of elements in array */
    int numElements;
    /* Lock for manipulation of scope */
    pthread_mutex_t scopeLock;
    /* Lock for manipulation of logical address */
    pthread_mutex_t addrLock;
};
```
- Each `PageTableEntry` corresponds to a symbol (variable/array) that is declared in the program. So essentially, the page table is a symbol table
- `name`, `type` are self-explanatory
- `localAddr` is the local address (in the library space) of the symbol
- `logicalAddr` is the logical address (assigned by the kernel) of the symbol in the memory assigned using `createMem()`

> We store the logical address directly in this table itself as we wanted simplicity + less storage space for the bookkeeping data structures that we used. Had we not kept it as a field in `PageTableEntry`, we would have had to store the offset of the variable in `PageTableEntry` and then have a separate table for mapping `localAddr` to `logicalAddr` and locate the symbol using the offset.

- `scope` keeps track of the scope of the symbol. There is a global variable called `currScope` which is incremented every time a new scope starts (see `startScope()` in **Functions**) and decremented every time the current scope ends (see `endScope()` in **Functions**). `symbol.scope` is assigned the value of `currScope` every time a new variable/array is declared. It can three kinds of values
	- `>=0` : symbol is part of some active scope
	- `-1` : symbol has been *marked* for deletion
	- `-2` : symbol has been *swept* or deleted (memory held by it was freed)
- `numElements` is the number of elements in the array if the symbol is an array
- `scopeLock` and `addrLock` are discussed in detail in the **Usage of Locks** section

### Additional Data Structures and Functions
#### Data Structures
All the book-keeping data structures that we use are encapsulated in the class `BookkeepingMem`. We allocated space for an instance of this class in  `createMem()` itself using `new` operator.
Here is an overview of the other book-keeping data structures used
```
class BookkeepingMem {
public:
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
    
    /* Starting address of memory created with createMem() */
    unsigned int *logicalMem;
    
    /* Global counter for local address */
    unsigned int localAddrCounter = 0;
    
    /* Global scope counter */
    int currScope = 0;
    
    /* Size of memory allocated */
    int sizeOfMem = 0;
    
    /* Free space left in memory */
    int freeMem = 0;
    
    /* Thread ID of the garbage collector thread */
    pthread_t gcTid;
    
    /* Mutex lock to be used during compaction done by garbage collector */
    pthread_mutex_t compactLock;
    
    /* Mutex lock to be used for printing log messages */
    pthread_mutex_t loggingLock;
    
    /* Time of start */
    timeval st;
};
```
- `freeIntervals` is a set of `{start, end}` intervals representing intervals in the logical address space that are free. `freeSpaces` is a set of `{size of free space, start index}` intervals representing holes (free spaces) in the logical address space stored in ascending order of the hole size.
 
> `freeIntervals` and `freeSpaces` carry a high overhead in terms of memory. However, together they are used for implementing (a) **best-fit allocation in logarithmic time** and (b) **freeing memory for a variable + coalescing smaller neighbouring holes** in logarithmic time. The algorithm is discussed in the **Functions** section in `findBestFitFreeSpace()` and `freeElementMem()`.

- `logicalMem`  is a pointer we always use to access the memory. `logicalMem` is a `unsigned int *` because we always access memory in chunks of 4 bytes (**word alignment**)
- `currScope` is incremented every time a new scope starts (see `startScope()` in **Functions**) and decremented every time the current scope ends (see `endScope()` in **Functions**).

 >It is this behaviour of `currScope` that removes the necessity of a global garbage collection stack. When a scope ends (indicated by the user calling `endScope()`), one can simply *mark* all the symbols in the table that have `symbol.scope == currScope` (we do it making `symbol.scope = -1`). Then upon running the garbage collector with `opt = 0`, it will start *sweeping* bottom to up, will *delete* (refer **Structure of Page Table** section for definition) the marked entries from the table (like popping from the stack) and free the memory held by them. It will stop popping as soon as it finds an unmarked entry (a symbol that has not gone out of scope).
 >Using this small optimisation we're able to avoid keeping a stack explicitly while achieving all of its functionality.

- `localAddrCounter` : Counter for local address that always increases as a multiple of 4
- Rest are self-explanatory

#### Functions
##### Basic
- `createMem(int size)` : Allocate separate memory for book-keeping data structures, initialize them and allocate `size`  bytes of memory for the user's purposes
- `createVar(char varName[32], DataType type)` : Checks if variable of this name already exists or not. If not, calls `findBestFitFreeSpace()` to find the best-fit allocation of memory for this variable. Adds a new page table entry corresponding to this variable. Returns a pointer to the page table entry (`PageTableEntry *`). 

>- Our free space assigning policy for symbols is as follows :
>	1. `Int` : starts at a word offset of 0 and size = 4 bytes. Same in case of arrays.
>	2. `MediumInt` : starts at a word offset of 0/1 and size = 3 bytes. In case of arrays, element size is 4 bytes.
>	3. `Char` : starts at a word offset of 0/1/2/3 and size = 1 byte. Same in case of arrays.
>	4. `Boolean` : starts at a word offset of 0/1/2/3 and size = 1 byte. In case of arrays, if Boolean array is of size 45, then it occupies ceil(45/8) = 6 bytes.
>- If a `MediumInt` is stored in bytes {0...2}, another `MediumInt` can only be stored in the next word, i.e., {4...6}, to maintain word alignment.
>- If a `MediumInt` is stored in bytes {0...2}, a `Char` can be stored in byte 3. It maintains word alignment and ensures efficient packing.
>- 4 different `Char`s or `Boolean`s can be stored in bytes 0,1,2 and 3.

- `createArr(char varName[32], DataType type, int elements)` : Checks if variable/array by this name already exists or not. If not, calls `findBestFitFreeSpace()` to find the best-fit allocation of memory for this array. Adds a new page table entry corresponding to this array. Returns a pointer to the page table entry (`PageTableEntry *`). 
- `assignVar(PageTableEntry *pte, int value)` : Assign `value` to the variable referenced by `pte`. Access to this variable is made in a four byte chunk.
- `assignArr(PageTableEntry *pte, int index, int value)` : Assign `value` to the array index `index`  of array referenced by `pte`. Access to this array element is made in a four byte chunk.
- `varValue(PageTableEntry *pte)` : Returns an `int` that stores the value at the logical address of the `PageTableEntry` that is passed as argument. Access to memory is made in chunk of four bytes.
- `arrValue(PageTableEntry *pte, int index)` : Returns an `int` that stores the value at an offset of `index`  from the logical address of the `PageTableEntry` that is passed as argument. Access to memory is made in chunk of four bytes.
- `freeElement(PageTableEntry *)` : Marks the passed `PageTableEntry` for deletion. It's associated memory will be freed later on by `gcRun()`.
- `diagnose()` : Prints the amount of free space left in memory and the time of for which program has been running.

##### Internal
- `findBestFitFreeSpace(int size)` : The algorithm is as follows
	- Find the smallest free space in memory that is greater than or equal to `size`. This is achieved in $O(\log N$) \[where $N$ is the number of free spaces in memory (roughly equal to the number of symbols)\] time by performing binary search on `freeSpaces`  (`freeSpaces.lower_bound()`). The  starting address of this free space is returned.
	- The found free space is deleted ($O(\log N)$) from `freeSpaces` and its corresponding element in `freeIntervals` too.
	- Two new free spaces are inserted ($O(\log N)$) to `freeSpaces` and `freeIntervals`, the remaining part of the word that the last byte of the variable/array occupies + the rest of the free space.
	- Example: We need 5 bytes for a `Char` array. The best-fit free space is {4, 15} (12 bytes). Thus, after giving 5 bytes from these 12 bytes to the `Char` array. {4, 15} is deleted and {9, 11} (remaining word) and {12, 15} (rest of the free space) are inserted.
- `freeElementMem(PageTableEntry *pte)` : Frees the memory associated with the variable or array referenced by `pte`. The free space thus created is inserted into the `freeIntervals` and `freeSpaces` sets appropriately. It always coalesces free spaces sharing boundaries appropriately.
- `assignValueInMem(int logicalAddr, int size, DataType type, int value)` : Does typechecking, accesses memory in chunk of four bytes, extracts required bytes through bitmasking, copies `value` and puts it back into memory by writing in chunk of four bytes.

##### Garbage Collection
- `startScope()` : User can call to indicate start of a scope. Recommended to be called before any function call. It increments `currScope`.
- `endScope()` : User can call to indicate end of a scope. Recommended to be called after function returns and return value (if any) has been stored. It *marks* variables that have gone out of scope (by doing linear search on the page table from bottom to top). Search stops as soon as a variable that is still in scope is encountered. Then it decrements `currScope`.
- `gcInit()` : Starts the garbage collector thread.
- `gcRunner()` {Internal} : Runner function for the garbage collector thread. Periodically (currently set to 10 ms for demo) calls `gcRun(1)` and `compactMem()`.
- `gcRun(int opt)` : This function has two modes of operation indicated by the argument `opt`
	- `gcRun(1)` :  Sweep through all entries in the page table, find the ones that have been marked and free the memory allocated to them. The entry is marked as deleted/freed by making `symbol.scope = -2`. User is recommended not to call this in their programs for garbage collection.
	- `gcRun(0)` : Simulates unwinding/popping from stack as it sweeps from the bottom of the table to the top, *deletes* (`numEntries--`) entries that have gone out of scope (`symbol.scope == -1`) and frees the memory held by them. Stops sweeping as soon as a symbol is encountered that has not gone out of scope.

>We chose to keep two modes of operation for the garbage collector instead of one. This is because we needed to provide the user with a call that would pop from the "stack" all the elements that have gone out of scope (most importantly after function returned). We could have had call that would just wake up the garbage collector thread itself, but that would incur additional overheads of sending a signal to the gc thread. Additionally, having the stack popping + freeing part run on the gc thread would basically mean iterating through the entire stack (as `createVar()` might happen in an interleaved manner) - this is redundant as the gc thread wakes up periodically anyway to do periodic clean up.

- `copyBlock(size, old_location, new_location)` {Internal} : Copies  `size` (betwen 0 and 4) number of bytes from `old_location` to `new_location`. Internally, all the memory accesses are done using `unsigned int *logicalMem`, thus in chunks of 4 bytes.
- `compactMem()` {To be made internal, available to user now only for demo} : See **Compaction logic**.

### Statistics of Garbage Collection
##### Running Time
- `demo1.cpp` :
    - Time taken without gc (avg over 100 runs) = **582.80 ms**
    - Time taken with gc (avg over 100 runs) = **720.38 ms**
    - Time taken with gc + calling gc after every function call for stack popping (avg over 100 runs) = **690.22 ms**
- `demo2.cpp` :
** with k = 9
    - Time taken without gc (avg over 100 runs) = **0.072 ms**
    - Time taken with gc (avg over 100 runs) = **0.148 ms**
    - Time taken with gc + calling gc after every function call for stack popping (avg over 100 runs) = **0.156 ms**
- `demo3.cpp` : 
    - Time taken without gc (avg over 100 runs) = **22.89 ms**
    - Time taken with gc (avg over 100 runs) = **27.28 ms**

>In all the three demos, it can be observed that running the program with garbage collection enabled takes longer than running it without it. However, the time difference is not significantly large and along with it we get more free memory to work with during execution.
>Garbage collection is quite fast as our `freeElementMem()` function frees the associated memory with a symbol in **logarithmic** time.

##### Memory Footprint
*NOTE : We have plotted Free Memory (in bytes) vs Time in execution of program (in ms) graphs. Thus, a dip in the plot means increase in memory footprint and a rise in the plot means decrease in footprint. Data points were obtained through calls to `diagnose()` which is called in demo\*.cpp whenever memory is allocated or freed.*

- `demo1.cpp` : This program has a high running time and higher memory requirements and thus the effect of garbage collection is very pronounced.
	- **Without garbage collection** : The program's memory footprint increases as it runs for longer, because memory used by it is never freed up
	- **With garbage collection** : The program's memory footprint increases first then the garbage collector wakes up and so footprint decreases, and this same behaviour is followed periodically. Thus, at the end we have significantly more free memory than the previous case. We save almost 1 MB of memory.
	- **With  garbage collection and stack popping by user (calls to `gcRun(0)`)** : This is pretty similar to the earlier case, except the running time is a little lesser, which might be because part of the garbage collection workload is handled by the `gcRun(0)` called by the user. Also, in this case, the entire memory is guaranteed to be freed at the end of the program if the user has called `gcRun(0)` at the end of every function (unlike the earlier case, where the entire memory is not guaranteed to be freed).

![[demo1.png]]
- `demo2.cpp` : This program has a very low running time and lesser memory requirements and thus the effect of garbage collection is not very pronounced.
	- **Without garbage collection** : The program's memory footprint increases as it runs for longer, because memory used by it is never freed up
	- **With garbage collection** : The program's memory footprint increases and the garbage collector doesn't even have the time to wake up (as it's a short program), and so memory is not freed up.
	- **With  garbage collection and stack popping by user (calls to `gcRun(0)`)** : In this case, the entire memory is guaranteed to be freed at the end of the program as the user has called `gcRun(0)` at the end of every function.

![[demo2.png]]
- `demo3.cpp` : This program has a very low running time and lesser memory requirements and thus the effect of garbage collection is not very pronounced.
	- **Without garbage collection** : The program's memory footprint increases as it runs for longer, because memory used by it is never freed up
	- **With garbage collection** : The program's memory footprint increases first then the garbage collector wakes up and so footprint decreases, and this same behaviour is followed periodically. Thus, at the end we have significantly more free memory than the previous case. We save almost 1.2 MB of memory.

![[demo3.png]]

### Compaction logic
Compaction is done by the garbage collection thread. After it runs `gcRun(1)` which does the garbage collection, the thread makes a call to  `compactMem()`.

>Since compaction of memory is an expensive process which stalls the actual program significantly, we do it only when **there is large hole of size >= 200 KB  in the memory allocated**.

If there is indeed a large hole in the memory, we do compaction using the following approach
- Create a table of `PageTableEntry` pointers pointing to the original `pageTable` just sorted by their `logicalAddr`.
- Iterate through the table top to down, call `freeElementMem()` (free its associated memory), call `findBestFitFreeSpace()` (assign new free space in memory).
- Copy the value stored in the variable/array elements from old location to new location in memory.

This entire process is expensive as it takes O(NlogK) time, where N is the number of page table entries and K is the number of free spaces in memory (which will also be nearly around N).

### Usage of Locks
- `compactLock` must be used as we cannot create variables or arrays during the compaction process. Also, we cannot be making calls to `freeElementMem()` from `gcRun()` while compaction is going on.
- `loggingLock` must be used for printing the logging statements in an order that makes sense and is readable and comprehensible.
- `symbol.scopeLock` is needed for every page (symbol) table entry as the garbage collector thread modifies the `symbol.scope`  from `gcRun()`, but the user may also call `gcRun()` .**
- `symbol.addrLock` is needed so that `assignVar()`, `assignArr()`, `varValue()`, `arrValue()` do not access/write the logical memory associated with the symbol while compaction is being done (which inevitably changes the logical address of the symbols).