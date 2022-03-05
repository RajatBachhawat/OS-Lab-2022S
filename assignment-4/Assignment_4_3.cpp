#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <wait.h>
#include <errno.h>
#include <iostream>

using namespace std;

#define INITIAL_NODES_LOWER 300
#define INITIAL_NODES_UPPER 500
#define MAX_JOB_TIME 250000000
#define THREAD_SLEEP_LOWER 200000000
#define THREAD_SLEEP_UPPER 500000000
#define THREAD_RUN_LOWER 10
#define THREAD_RUN_UPPER 20
#define MAX_NODES 1000
#define SHM_KEY 100

pthread_mutex_t num_nodes_lock;

int in1 = 0, in2 = 0, in3 = 0, in4 = 0, in5;

struct Node {
    int arr_id;
    int job_id;
    int completion_time;
    int dependent_jobs[MAX_NODES];
    int num_dependent;
    pthread_mutex_t lock;
    int status;
    int children;
};

struct shared_mem {
    int root;
    int num_nodes;
    Node tree[MAX_NODES];
    
};

void init_node(Node* n, int index)
{
    n->job_id = 1+rand()%(int)1e8;
    n->arr_id = index;
    n->completion_time = rand()%(MAX_JOB_TIME+1);
    n->status = 0;
    n->num_dependent = 0;
    n->children = 0;
    /* Initialize the mutex lock variable */
    if(pthread_mutex_init(&n->lock, NULL) != 0) {
        fprintf(stderr, "Pthread mutex init error %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void init_tree(shared_mem *shaddr)
{
    shaddr->root = 0;
    shaddr->num_nodes = 1;
    /* Initialize root node */
    init_node(&shaddr->tree[0], 0);
    int num_nodes = INITIAL_NODES_LOWER + rand()%(INITIAL_NODES_UPPER-INITIAL_NODES_LOWER+1);

    for(int addnode = 1; addnode < num_nodes; addnode++)
    {   
        /* Initialize new node */
        init_node(&shaddr->tree[addnode], addnode);

        int parent = rand()%shaddr->num_nodes;
        int num_dep = shaddr->tree[parent].num_dependent;
        /* Add this node/job as a dependent_job of parent */
        shaddr->tree[parent].dependent_jobs[num_dep] = addnode;
        shaddr->tree[parent].num_dependent++;
        shaddr->tree[parent].children++;
        /* Set parent status as 1 (Waiting) */
        shaddr->tree[parent].status = 1;
         /* Increment the number of nodes in tree */
        shaddr->num_nodes++;
    }
}

int dfs(shared_mem *shaddr, int currnode, int parent){
    int n = shaddr->tree[currnode].num_dependent;
    
    for(int i=0; i<n; i++)
    {
        dfs(shaddr, shaddr->tree[currnode].dependent_jobs[i], currnode);
    }

    in1++;
    pthread_mutex_lock(&shaddr->tree[currnode].lock);

    if(shaddr->tree[currnode].status == 0){
        /* Ready */
        shaddr->tree[currnode].status = 2;
        pthread_mutex_unlock(&shaddr->tree[currnode].lock);
        in5--;

        /* Executing */
        cout<<"Job #"<<shaddr->tree[currnode].arr_id<<" started\n";
        timespec ct = {0, shaddr->tree[currnode].completion_time};
        nanosleep(&ct, NULL);
        /* Terminated */
        cout<<"Job #"<<shaddr->tree[currnode].arr_id<<" terminated, ran for "<<shaddr->tree[currnode].completion_time<<" nanos\n";
        shaddr->tree[currnode].status =3;
        if(parent != -1)
        {    
            in2++;
            pthread_mutex_lock(&shaddr->tree[parent].lock);
            shaddr->tree[parent].children--;
            if(shaddr->tree[parent].children == 0)
                shaddr->tree[parent].status = 0;
            pthread_mutex_unlock(&shaddr->tree[parent].lock);
            in2--;
        }
        return 1;
    }
    else 
    {
        if(shaddr->tree[currnode].status == 2 || shaddr->tree[currnode].status == 3)
        {
            pthread_mutex_unlock(&shaddr->tree[currnode].lock);
            in4--;
            return 0;
        }
        else 
        {
            pthread_mutex_unlock(&shaddr->tree[currnode].lock);
            in3--;
        }
    }
    return 0;
    
}

void *worker_runner(void *param){
    
    shared_mem* shaddr = (shared_mem*)param;
    while(1){
        dfs(shaddr, 0, -1);
        if(shaddr->tree[0].status == 3)
        {
            break;
        }
    }
    cout<<"consumer exit\n";
    pthread_exit(NULL);
    return NULL;
}

void *producer_runner(void *param){
    time_t start_time = time(NULL);
    /* Time for which the thread runs */
    int run_time = THREAD_RUN_LOWER+rand()%(THREAD_RUN_UPPER - THREAD_RUN_LOWER+1);

    shared_mem* shaddr = (shared_mem*)param;

    while(1)
    {
        /* If thread has been running for more time than run_time, exit */
        if(difftime(time(NULL), start_time) > run_time){
            cout<<"producer exit\n";
            break;
        }

        /* Choose a random node for adding a child */
        int parent_idx = rand()%shaddr->num_nodes;

        in1++;
        /* Start critical section for chosen node */
        pthread_mutex_lock(&shaddr->tree[parent_idx].lock);
        if(shaddr->tree[parent_idx].status == 0 || shaddr->tree[parent_idx].status == 1)
        {
            /* Start critical section for updation of num_nodes */
            pthread_mutex_lock(&num_nodes_lock);
            int idx = shaddr->num_nodes;
            shaddr->num_nodes++;
            pthread_mutex_unlock(&num_nodes_lock);
            /* End critical section for updation of num_nodes */

            init_node(&shaddr->tree[idx], idx);
            shaddr->tree[parent_idx].dependent_jobs[shaddr->tree[parent_idx].num_dependent++] = idx;
            shaddr->tree[parent_idx].status = 1;
            shaddr->tree[parent_idx].children++;
            printf("Added Node #%d, Parent Node #%d\n", idx, parent_idx);
            pthread_mutex_unlock(&shaddr->tree[parent_idx].lock);
            in2--;
        }
        else 
        {
            pthread_mutex_unlock(&shaddr->tree[parent_idx].lock);
            in3--;
            continue;
        }
    
        /* Sleep for random time between 200ms and 500ms */
        timespec sleep_time = {0, THREAD_SLEEP_LOWER + rand()%(THREAD_SLEEP_UPPER - THREAD_SLEEP_LOWER + 1)};
        nanosleep(&sleep_time, NULL);
        /* End critical section for chosen node */
    }
    pthread_exit(NULL);
    return NULL;
}

void* debug(void* param)
{
    while(1)
    {
        sleep(1);
        cout<<in1<<" "<<in2<<" "<<in5<<" "<<in3<<" "<<in4<<"\n";
    }
    return NULL;
}
void* debugP(void* param)
{
    while(1)
    {
        sleep(1);
        cout<<in1<<" "<<in2<<" "<<in3<<"\n";
    }
    return NULL;
}
int main(){
    
    int shmid = shmget(SHM_KEY, sizeof(shared_mem), IPC_CREAT | 0600); 
    shared_mem *shaddr = (shared_mem *)shmat(shmid, NULL, 0);
    
    /* Generate a random tree with 300 to 500 nodes in shared memory */
    init_tree(shaddr);
    
    /* Number of producer and worker threads as user input */
    int num_producers, num_workers;
    printf("Enter the number of producer threads: ");
    scanf("%d", &num_producers);
    printf("Enter the number of worker threads: ");
    scanf("%d", &num_workers);

    /* Fork Process B that spawns the workers */
    pid_t pid = fork();
    if(pid < 0){
        fprintf(stderr, "Fork error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Process B, worker spawner */
    if(pid == 0){
        in1 = 0;
        in2 = 0;
        pthread_t tdeb;
        pthread_attr_t attrd;
        pthread_attr_init(&attrd); /* Get default attributes */
        pthread_create(&tdeb, &attrd, debug, NULL); /* Create the worker thread */

        pthread_t *tid = (pthread_t *)malloc(num_workers*sizeof(pthread_t));
        pthread_attr_t attr;
        pthread_attr_init(&attr); /* Get default attributes */
        for(int i=0; i<num_workers; i++){
            pthread_create(&tid[i], &attr, worker_runner, shaddr); /* Create the worker thread */
        }
        for(int i=0; i<num_workers; i++){
            pthread_join(tid[i],0); /* Create the producer thread */
        }
        cout<<"child-here\n";
        exit(EXIT_SUCCESS);
    }

    pthread_t tdeb;
        pthread_attr_t attrd;
        pthread_attr_init(&attrd); /* Get default attributes */
        pthread_create(&tdeb, &attrd, debugP, NULL); /* Create the worker thread */

    /* Process A, producer spawner */
    pthread_t *tid = (pthread_t *)malloc(num_producers * sizeof(pthread_t));
    pthread_attr_t attr;
    pthread_attr_init(&attr); /* Get default attributes */

    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutex_init(&num_nodes_lock, &mattr); /* Initialize mutex lock for shared variable num_nodes */

    for(int i=0; i<num_producers; i++){
        pthread_create(&tid[i], &attr, producer_runner, shaddr); /* Create the producer thread */
    }
    for(int i=0; i<num_producers; i++){
        pthread_join(tid[i],0); /* Create the producer thread */
    }
    wait(NULL);

    return 0;
}
