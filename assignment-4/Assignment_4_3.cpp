#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <wait.h>
#include <errno.h>

#define INITIAL_NODES_LOWER 300
#define INITIAL_NODES_UPPER 500
#define MAX_JOB_TIME 250000000L
#define THREAD_SLEEP_LOWER 200000000L
#define THREAD_SLEEP_UPPER 500000000L
#define THREAD_RUN_LOWER 10
#define THREAD_RUN_UPPER 20
#define MAX_NODES 1000
#define MAX_PRODUCERS 50
#define MAX_WORKERS 50
#define SHM_KEY 100

pthread_mutex_t num_nodes_lock;

struct Node {
    int arr_id; /* Index in tree array */
    int job_id; /* Job ID */
    int completion_time; /* Time to complete job */
    int dependent_jobs[MAX_NODES]; /* List of arr_id's of dependent jobs aka children nodes */
    int num_children; /* Number of children added (this never decreases) */
    int num_dependencies; /* Number of children added (this can decrease or increase, is dynamic) */
    pthread_mutex_t lock; /* Mutex lock for updation/reading */
    /*
    status = 0 : independent job (number of children = 0)
    status = 1 : dependent/waiting job (number of children > 0)
    status = 2 : ready job (has been acquired by a consumer for running)
    status = 3 : terminated job (has been finished by consumer)
    */
    int status;
};

struct shared_mem {
    int root;
    int num_nodes;
    Node tree[MAX_NODES];
};

int init_node(Node* n, int index)
{
    n->job_id = 1+rand()%(int)1e8; /* Assign random job id between 1 and 1e8 */
    n->arr_id = index;
    n->completion_time = rand()%(MAX_JOB_TIME+1); /* Assign random completion time between 0 and 250ms */
    n->status = 0;
    n->num_children = 0;
    n->num_dependencies = 0;
    
    /* Initialize the mutex attributes */
    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
    /* Initialize the mutex lock variable */
    if(pthread_mutex_init(&n->lock, &mattr) != 0) {
        fprintf(stderr, "Pthread mutex init error %s\n", strerror(errno));
        pthread_mutexattr_destroy(&mattr);
        return -1;
    }
    pthread_mutexattr_destroy(&mattr);
    return 0;
}

int init_tree(shared_mem *shaddr)
{
    shaddr->root = 0;
    shaddr->num_nodes = 1;
    /* Initialize root node */
    if(init_node(&shaddr->tree[0], 0) < 0){
        return -1;
    }
    int num_nodes = INITIAL_NODES_LOWER + rand()%(INITIAL_NODES_UPPER-INITIAL_NODES_LOWER+1);

    for(int addnode = 1; addnode < num_nodes; addnode++)
    {   
        /* Initialize new node */
        if(init_node(&shaddr->tree[addnode], addnode) < 0){
            /* Destroy the mutex locks initialized till now */
            for(int i=0; i<addnode; i++){
                pthread_mutex_destroy(&shaddr->tree[i].lock);
            }
            return -1;
        }

        int parent = rand()%shaddr->num_nodes;
        int num_dep = shaddr->tree[parent].num_children;
        /* Add this node/job as a dependent_job of parent */
        shaddr->tree[parent].dependent_jobs[num_dep] = addnode;
        shaddr->tree[parent].num_children++;
        shaddr->tree[parent].num_dependencies++;
        /* Set parent status as 1 (Waiting) */
        shaddr->tree[parent].status = 1;
        /* Increment the number of nodes in tree */
        shaddr->num_nodes++;
    }
    return 0;
}

void dfs(shared_mem *shaddr, int currnode, int parent){

    pthread_mutex_lock(&shaddr->tree[currnode].lock);
    /* If current job is ready (acquired by a different consumer) or terminated, return */
    if(shaddr->tree[currnode].status == 2 || shaddr->tree[currnode].status == 3)
    {
        pthread_mutex_unlock(&shaddr->tree[currnode].lock);
        return;
    }
    /* If current job is independent and no acquired by any other consumer already, acquire and run it */
    if(shaddr->tree[currnode].status == 0){
        /* Ready */
        shaddr->tree[currnode].status = 2;
        printf("Job with node #%d and job id %d started\n", shaddr->tree[currnode].arr_id, shaddr->tree[currnode].job_id);
        pthread_mutex_unlock(&shaddr->tree[currnode].lock);

        /* Executing */
        timespec ct = {0, shaddr->tree[currnode].completion_time};
        nanosleep(&ct, NULL);

        /* Terminated */
        pthread_mutex_lock(&shaddr->tree[currnode].lock);
        printf("Job with node #%d and job id %d terminated, Ran for %dms\n", shaddr->tree[currnode].arr_id, shaddr->tree[currnode].job_id, shaddr->tree[currnode].completion_time/1000000);
        shaddr->tree[currnode].status =3;
        pthread_mutex_unlock(&shaddr->tree[currnode].lock);

        /* Update parent by removing child */
        if(parent != -1)
        {
            /* Start of critical section for updation of parent status */
            pthread_mutex_lock(&shaddr->tree[parent].lock);
            shaddr->tree[parent].num_dependencies--;
            if(shaddr->tree[parent].num_dependencies == 0)
                shaddr->tree[parent].status = 0;
            pthread_mutex_unlock(&shaddr->tree[parent].lock);
            /* End of critical section for updation of parent status */
        }
        return;
    }
    pthread_mutex_unlock(&shaddr->tree[currnode].lock);
    
    /* If the job is dependent on other jobs (aka has children), do DFS on the children */
    for(int i=0; i<shaddr->tree[currnode].num_children; i++)
    {
        dfs(shaddr, shaddr->tree[currnode].dependent_jobs[i], currnode);
    }
}

void *worker_runner(void *param){
    /* Making thread id the seed */
    srand(pthread_self());

    shared_mem* shaddr = (shared_mem*)param;

    /* Repeatedly DFS in the tree to consume jobs, until root (arr_id = 0) has been consumed */
    while(!(shaddr->tree[0].status == 2 || shaddr->tree[0].status == 3)){
        dfs(shaddr, 0, -1);
    }
    pthread_exit(NULL);
    return NULL;
}

void *producer_runner(void *param){

    /* Making thread id the seed */
    srand(pthread_self());
    time_t start_time = time(NULL);
    /* Time for which the thread runs */
    int run_time = THREAD_RUN_LOWER+rand()%(THREAD_RUN_UPPER - THREAD_RUN_LOWER+1);

    shared_mem* shaddr = (shared_mem*)param;

    while(1)
    {
        /* If thread has been running for more time than run_time, exit */
        if(difftime(time(NULL), start_time) > run_time){
            break;
        }

        /* Choose a random node for adding a child */
        int parent_idx = rand()%shaddr->num_nodes;

        /* Start critical section for chosen node */
        pthread_mutex_lock(&shaddr->tree[parent_idx].lock);
        if(shaddr->tree[parent_idx].status == 0 || shaddr->tree[parent_idx].status == 1)
        {
            /* Start critical section for updation of num_nodes */
            pthread_mutex_lock(&num_nodes_lock);
            int idx = shaddr->num_nodes;
            shaddr->num_nodes++;
            /* End critical section for updation of num_nodes */
            pthread_mutex_unlock(&num_nodes_lock);

            if(init_node(&shaddr->tree[idx], idx) < 0){
                pthread_exit(NULL);
            }
            shaddr->tree[parent_idx].dependent_jobs[shaddr->tree[parent_idx].num_children++] = idx;
            shaddr->tree[parent_idx].num_dependencies++; 
            shaddr->tree[parent_idx].status = 1;
            printf("Added Node #%d with job id %d, Parent Node #%d\n", idx, shaddr->tree[idx].job_id, parent_idx);
            /* End critical section for chosen node */
            pthread_mutex_unlock(&shaddr->tree[parent_idx].lock);

            /* Sleep for random time between 200ms and 500ms */
            timespec sleep_time = {0, THREAD_SLEEP_LOWER + rand()%(THREAD_SLEEP_UPPER - THREAD_SLEEP_LOWER + 1)};
            nanosleep(&sleep_time, NULL);
        }
        else 
        {
            /* End critical section for chosen node */
            pthread_mutex_unlock(&shaddr->tree[parent_idx].lock);
        }
    }
    pthread_exit(NULL);
    return NULL;
}

/* Cleanup work before exitting */
void exit_cleanup(shared_mem *shaddr, int shmid){
    /* Destroy the mutex locks */
    pthread_mutex_destroy(&num_nodes_lock);
    for(int i=0; i<MAX_NODES; i++){
        pthread_mutex_destroy(&shaddr->tree[i].lock);
    }
    /* Detach the shared memory segment from the address space of this process */
    shmdt(shaddr);
    /* Remove the shared memory segment */
    shmctl(shmid, IPC_RMID, NULL);
}

int main(){

    /* Making thread id the seed */
    srand(pthread_self());
    
    int shmid = shmget(SHM_KEY, sizeof(shared_mem), IPC_CREAT | 0600);
    if(shmid < 0){
        fprintf(stderr, "Error in allocating shared memory: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    shared_mem *shaddr = (shared_mem *)shmat(shmid, NULL, 0);
    
    /* Generate a random tree with 300 to 500 nodes in shared memory */
    if(init_tree(shaddr) < 0){
        /* Destroy the global mutex lock */
        pthread_mutex_destroy(&num_nodes_lock);
        /* Detach the shared memory segment from the address space of this process */
        shmdt(shaddr);
        /* Remove the shared memory segment */
        shmctl(shmid, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }
    
    /* Number of producer and worker threads as user input */
    int num_producers, num_workers;
    printf("Enter the number of producer threads:\n");
    scanf("%d", &num_producers);
    if(num_producers <= 0){
        fprintf(stderr, "Number of producers must be positive\n");
        exit_cleanup(shaddr, shmid);
        exit(EXIT_FAILURE);
    }
    printf("Enter the number of worker threads:\n");
    scanf("%d", &num_workers);
    if(num_workers <= 0){
        fprintf(stderr, "Number of workers must be positive\n");
        exit_cleanup(shaddr, shmid);
        exit(EXIT_FAILURE);
    }

    /* Fork Process B that spawns the workers */
    pid_t pid = fork();
    if(pid < 0){
        fprintf(stderr, "Fork error: %s\n", strerror(errno));
        /* Cleanup work */
        /* Destroy the mutex locks */
        pthread_mutex_destroy(&num_nodes_lock);
        for(int i=0; i<MAX_NODES; i++){
            pthread_mutex_destroy(&shaddr->tree[i].lock);
        }
        /* Detach the shared memory segment from the address space of this process */
        shmdt(shaddr);
        /* Remove the shared memory segment */
        shmctl(shmid, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }

    /* Process B, worker spawner */
    if(pid == 0){

        pthread_t *tid = (pthread_t *)malloc(num_workers*sizeof(pthread_t));
        pthread_attr_t attr;
        pthread_attr_init(&attr); /* Get default attributes */
        pthread_attr_destroy(&attr);
        
        for(int i=0; i<num_workers; i++){
            pthread_create(&tid[i], &attr, worker_runner, shaddr); /* Create the worker threads */
        }
        for(int i=0; i<num_workers; i++){
            pthread_join(tid[i],0); /* Join the worker thread */
        }
        
        /* Detach the shared memory segment from the address space of this process */
        shmdt(shaddr);
        exit(EXIT_SUCCESS);
    }

    /* Process A, producer spawner */
    pthread_t *tid = (pthread_t *)malloc(num_producers * sizeof(pthread_t));
    pthread_attr_t attr;
    pthread_attr_init(&attr); /* Get default attributes */
    pthread_attr_destroy(&attr);

    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutex_init(&num_nodes_lock, &mattr); /* Initialize mutex lock for shared variable num_nodes */
    pthread_mutexattr_destroy(&mattr);

    for(int i=0; i<num_producers; i++){
        pthread_create(&tid[i], &attr, producer_runner, shaddr); /* Create the producer threads */
    }
    for(int i=0; i<num_producers; i++){
        pthread_join(tid[i],0); /* Join the producer threads */
    }

    waitpid(-1, NULL, 0); /* Wait for Process B to terminate */

    exit_cleanup(shaddr, shmid);

    return 0;
}
