#ifndef SYS_CALL
#define SYS_CALL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <fcntl.h>
#include <dirent.h>
#include <termios.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>

using namespace std;

#define MAXARGS 128
#define MAXLINE 200
#define HISTSIZE 100000
#define DSPLHISTSIZE 1000

inline int getCharMap(char ch);
extern unordered_map<char,int>charDict;
/* Data Structures */
struct node 
{
    node* child[100];
    int index;
    vector<int>indices;
    node():index(-1)
    {
        for(int i = 0; i<100; i++)
            child[i]=NULL;
    }
    node(int ind):index(ind)
    {
        for(int i = 0; i<100; i++)
            child[i]=NULL;

    }
};

struct trie 
{
    node* root;
    trie()
    {
        root = new node();
    }
    void add(string& s, int ind)
    {
        node* curr = root;
        size_t sz = s.size();   
        for(int i=0; i<sz;i++)
        {
            
            int x = getCharMap(s[i]);
            if(curr->child[x]==NULL)
            {
                curr->child[x] =  new node(ind);
                
            }
            else 
            {
                if(ind>(curr->child[x])->index)
                    (curr->child[x])->index = ind;
            }
            curr = curr->child[x];
            curr->indices.push_back(ind);
        }
    }
    int search(string& s)
    {
        node* curr = root;
        size_t sz = s.size();   
        for(int i=0; i<sz;i++)
        {
            int x = getCharMap(s[i]);
            if(curr->child[x]==NULL)
            {
                return -1;
            }
            curr = curr->child[x];
        }   
        return curr->index;
    }
    void searchMult(string &s, int& maxlen,  vector<int>&maxindices)
    {
        node* curr = root;
        size_t sz = s.size();   
        for(int i=0; i<sz;i++)
        {
            int x = getCharMap(s[i]);
            if(curr->child[x]==NULL)
            {
                return;
            }
            curr = curr->child[x];
            if(i+1==maxlen)
            {
                maxindices.insert(maxindices.end(), curr->indices.begin(), curr->indices.end());

            }
            else 
            if(i+1>maxlen)
            {
                maxlen = i+1;
                maxindices = curr->indices;
            }
        }   
        return;
    }
};
/* Our own error-handling functions */
void unix_error(const char *msg);
void app_error(const char *msg);

/* Unix I/O wrappers */
int Open(const char *pathname, int flags, mode_t mode);
ssize_t Read(int fd, void *buf, size_t count);
ssize_t Write(int fd, const void *buf, size_t count);
off_t Lseek(int fildes, off_t offset, int whence);
void Close(int fd);
int Select(int  n, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
int Dup2(int fd1, int fd2);
int Pipe(int pipefd[2]);
void Stat(const char *filename, struct stat *buf);
void Fstat(int fd, struct stat *buf) ;

/* Signal wrappers */
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/* Process control wrappers */
pid_t Fork(void);
void Execve(const char *filename, char *const argv[], char *const envp[]);
pid_t Wait(int *status);
pid_t Waitpid(pid_t pid, int *iptr, int options);
void Kill(pid_t pid, int signum);

/* Directory wrappers */
DIR *Opendir(const char *name);
struct dirent *Readdir(DIR *dirp);
int Closedir(DIR *dirp);

// Functions for history
void startShell(vector<string>& comms);
void stopShell(vector<string>& comms);
void displayHist(vector<string>& comms);
void addToHist(vector<string>& comms, string& command);
int searchInHist(int count, int key);
void addsubstrs(string& s, int ind);
inline int getCharMap(char ch)
{
    if(ch>='A' && ch<='Z')
        ch+='a'-'A';
    if(ch>='a' && ch<='z')  
        return ch-'a';
    return charDict[ch];
}
#endif
