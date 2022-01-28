#include "syscall.h"

extern vector<string> commands;

/* Data Structures*/

trie historyTrie;
unordered_map<char,int>charDict;


/************************** 
 * Error-handling functions
 **************************/
void unix_error(const char *msg) /* Unix-style error */
{
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    exit(0);
}

void app_error(const char *msg) /* Application error */
{
    fprintf(stderr, "%s\n", msg);
    exit(0);
}

/************************************
 * Wrapper for Unix signal function 
 ***********************************/

/* $begin sigaction */
handler_t *Signal(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* Block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* Restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
	unix_error("Signal error");
    return (old_action.sa_handler);
}
/* $end sigaction */

/*********************************************
 * Wrappers for Unix process control functions
 ********************************************/
pid_t Fork(void) 
{
    pid_t pid;

    if ((pid = fork()) < 0)
	unix_error("Fork error");
    return pid;
}

void Execvp(const char *filename, char *const *argv) 
{
    if (execvp(filename, argv) < 0)
	unix_error("Execvp error");
}

pid_t Wait(int *status) 
{
    pid_t pid;

    if ((pid  = wait(status)) < 0)
	unix_error("Wait error");
    return pid;
}

pid_t Waitpid(pid_t pid, int *iptr, int options) 
{
    pid_t retpid;

    if ((retpid  = waitpid(pid, iptr, options)) < 0) 
	unix_error("Waitpid error");
    return(retpid);
}

void Kill(pid_t pid, int signum) 
{
    int rc;

    if ((rc = kill(pid, signum)) < 0)
	unix_error("Kill error");
}

/********************************
 * Wrappers for Unix I/O routines
 ********************************/

int Open(const char *pathname, int flags, mode_t mode) 
{
    int rc;

    if ((rc = open(pathname, flags, mode))  < 0)
	unix_error("Open error");
    return rc;
}

ssize_t Read(int fd, void *buf, size_t count) 
{
    ssize_t rc;

    if ((rc = read(fd, buf, count)) < 0) 
	unix_error("Read error");
    return rc;
}

ssize_t Write(int fd, const void *buf, size_t count) 
{
    ssize_t rc;

    if ((rc = write(fd, buf, count)) < 0)
	unix_error("Write error");
    return rc;
}

off_t Lseek(int fildes, off_t offset, int whence) 
{
    off_t rc;

    if ((rc = lseek(fildes, offset, whence)) < 0)
	unix_error("Lseek error");
    return rc;
}

void Close(int fd) 
{
    int rc;

    if ((rc = close(fd)) < 0)
	unix_error("Close error");
}

int Select(int  n, fd_set *readfds, fd_set *writefds,
	   fd_set *exceptfds, struct timeval *timeout) 
{
    int rc;

    if ((rc = select(n, readfds, writefds, exceptfds, timeout)) < 0)
	unix_error("Select error");
    return rc;
}

int Dup2(int fd1, int fd2) 
{
    int rc;

    if ((rc = dup2(fd1, fd2)) < 0)
	unix_error("Dup2 error");
    return rc;
}

int Pipe(int pipefd[2])
{
    int rc;

    if((rc = pipe(pipefd)) < 0)
    unix_error("Pipe error");
    return rc;
}

void Stat(const char *filename, struct stat *buf) 
{
    if (stat(filename, buf) < 0)
	unix_error("Stat error");
}

void Fstat(int fd, struct stat *buf) 
{
    if (fstat(fd, buf) < 0)
	unix_error("Fstat error");
}

/*********************************
 * Wrappers for directory function
 *********************************/

DIR *Opendir(const char *name) 
{
    DIR *dirp = opendir(name); 

    if (!dirp)
        unix_error("opendir error");
    return dirp;
}

struct dirent *Readdir(DIR *dirp)
{
    struct dirent *dep;
    
    errno = 0;
    dep = readdir(dirp);
    if ((dep == NULL) && (errno != 0))
        unix_error("readdir error");
    return dep;
}

int Closedir(DIR *dirp) 
{
    int rc;

    if ((rc = closedir(dirp)) < 0)
        unix_error("closedir error");
    return rc;
}

void startShell(vector<string>&comms)
{
    // Initialising Character Dictionary
    string allchars = "1234567890!@#$%^&*()-=[];',./_+{}:\"<>?|\\ \n";
    for(int i = 0; i<allchars.size();i++)
        charDict[allchars[i]]=26+i;
    // Opening File
    fstream historyFile;
    historyFile.open("history.txt", ios::in);
    string curr;
    while(getline(historyFile,curr))
    {
        comms.push_back(curr);
       // cout<<curr<<"\n";
    }
    int n = comms.size();
    for(int i=0; i<n/2;i++)
    {
        swap(comms[i],comms[n-i-1]);
    }
    for(int i =0; i<comms.size();i++)
    {
        addsubstrs(comms[i],i);
    }

}

void stopShell(vector<string>&comms)
{
    fstream historyFile;
    historyFile.open("history.txt", ios::out);
    string curr;
    int count = HISTSIZE;
    while(comms.size()>0 && count--)
    {
        historyFile<<comms.back()<<"\n";
        comms.pop_back();
        
    }

}

void addsubstrs(string& s, int ind)
{
    size_t n = s.size();
    for(int i=0;i <n; i++)
    {
        string suffix = s.substr(i,n-i);
        historyTrie.add(suffix,ind);
    }
}

void displayHist(vector<string>& comms)
{
    size_t sz = comms.size();
    int total = max(0,(int)sz-DSPLHISTSIZE);
    for(int i = total ; i< sz; i++)
    {
        cout<<sz-i<<": "<<comms[i]<<"\n";
    }
}
void addToHist(vector<string>& comms, string& command)
{
    if(command.size()==1)
        return;
    command = command.substr(0,command.size()-1);
    comms.push_back(command);
    addsubstrs(command,comms.size()-1);
}
int searchInHist(int count, int key)
{
    string searchstr;
    cout<<"Enter Search String: ";
    getline(cin,searchstr);
    int ind = historyTrie.search(searchstr);
    if(ind==-1 || ind<commands.size()-HISTSIZE) // Also add if minimum index not in top 100000
    {
        vector<int>maxinds;
        int maxlen = 0;
        for(int i=0;i<searchstr.size();i++)
        {
            string searchval = searchstr.substr(i,searchstr.size()-i);
            historyTrie.searchMult(searchval,maxlen,maxinds);
        }
        bool currpresent = 0;
        if(maxlen <=2)
        {
            cout<<"Command Not Found\n";
        }
        else 
        {
            bool present = false;   
            unordered_map<int,int>alreadyPresent;
            for(auto it:maxinds)
            {
                if(it>=commands.size()-HISTSIZE && !alreadyPresent[it])
                    cout<<commands[it]<<"\n",currpresent = true,alreadyPresent[it]=1;
            }
            if(currpresent)
                cout<<"Maximum length of substring match: "<<maxlen<<"\n";
            else 
                cout<<"Command Not Found\n";
        }
    }
    else 
    {
        cout<<"Exact Match Found\nCommand: "<<commands[ind]<<"\n";
    }
    return 1;
}
