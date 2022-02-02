/* $begin shellmain */
#include "syscall.h"
#include "termraw.h"
#include <sys/inotify.h>
#include <poll.h>

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

pid_t pidout;
int verbose = 0;
int running;
set<pid_t>pids;
extern vector<string> commands;
int flag;
char* outputFile;
int watchcmd = 0;

struct cmdlineProps 
{   
    int rfd;
    int wfd; 
    cmdlineProps() : rfd(STDIN_FILENO),wfd(STDOUT_FILENO) {}
};
struct watchCommand
{
    char* argv;
    cmdlineProps prop;
};

/* Function prototypes */
void eval(char *cmdline);
void parseline(char *buf, char **argv, cmdlineProps &prop);
int watchParser( char* buf, watchCommand*);
void watcheval(int sz, watchCommand* wcs);

/* 
 * sigint_handler - SIGINT handler. The kernel sends a SIGINT whenver
 *     the user types ctrl-c at the keyboard. Simply catch and return.
 */
void sigint_handler(int sig) 
{
    running = 0;
    if (verbose)
	printf("sigint_handler: shell caught SIGINT\n");
}

/*
 * sigtstp_handler - SIGTSTP handler. The kernel sends a SIGSTP whenver
 *     the user types ctrl-z at the keyboard. Simply catch and return.
 */ 
void sigtstp_handler(int sig) 
{
    if (verbose)
	printf("sigtstp_handler: shell caught SIGTSTP\n");
}

/* 
 * sigchld_handler - SIGCHLD handler. The kernel sends a SIGCHLD 
 *     whenever a child process (job) terminates (becomes a zombie) or 
 *     stops  because it received a SIGTSTP signal (ctrl-z). The handler 
 *     reaps all available zombie jobs, but doesn't wait for any
 *     nonzombie jobs.
 */

void sigchld_handler(int sig) 
{
    pid_t pid;
    int status;
  
    if (verbose)
	    printf("sigchld_handler: entering \n");

    /* 
     * Reap any zombie jobs.
     * The WNOHANG here is important. Without it, the 
     * the handler would wait for all running or stopped BG 
     * jobs to terminate, during which time the shell would not
     * be able to accept input. 
     */
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) 
    {
        if(pids.find(pid)!=pids.end())
        {
            pids.erase(pid);
            if(flag == 0)
                running = 0;
        }
        if(pids.size()==0)
            running = 0;
        if (verbose)
            printf("sigchld_handler: job %d deleted\n", pid);
    }

    /* 
     * Check for normal loop termination. 
     * This is a little tricky. For our purposes, 
     * the waitpid loop terminates normally for one of
     * two reasons: (a) there are no children left 
     * (pid == -1 and errno == ECHILD) or (b) there are
     * still children left, but none of them are zombies (pid == 0).
     */
    if (!((pid == 0) || (pid == -1 && errno == ECHILD)))
	unix_error("sigchld_handler wait error");

    if (verbose)
	printf("sigchld_handler: exiting\n");

    return;
}

void clear(char *buf){
    for(int i=0; i<MAXFILENAMELEN; i++)
        buf[i]='\0';
}

void longest_common_prefix(char *lcp, char *s){
    int i = 0;
    while(lcp[i]!='\0' && s[i]!='\0'){
        if(lcp[i]!=s[i]){
            break;
        }
        i++;
    }
    while(lcp[i]!='\0'){
        lcp[i]='\0';
        i++;
    }
}

int autocomplete(char *line_buf){
    char ch;
    char lcp[MAXFILENAMELEN];
    char candidates[MAXDIRSIZE][MAXFILENAMELEN];
    char *buf;
    
    clear(lcp);
    char *ptr = line_buf;
    buf = ptr;
    char prevch = '\0';
    while((ch = *ptr)!='\0'){
        if(prevch == ' ' && ch != ' '){
            buf = ptr;
        }
        prevch = ch;
        ptr++;
    }
    if(*(ptr-1) == ' '){
        buf = ptr;
    }
    
    int i = 0;
    int num_matches = 0;
    DIR *d;
    struct dirent *dir;
    d = opendir(".");

    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if(dir->d_type == DT_REG){
                char *filename = dir->d_name;
                if(strstr(filename, buf) - filename == 0){
                    if(num_matches == 0)
                        strncpy(lcp, filename, MAXFILENAMELEN);
                    else
                        longest_common_prefix(lcp, filename);
                    strncpy(candidates[num_matches++], filename, MAXFILENAMELEN);
                }
            }
        }
        closedir(d);
    }

    char *extra_match = lcp + strlen(buf);
    if(strlen(extra_match) > 0){
        printf("%s", extra_match);
        strncpy(buf, lcp, MAXFILENAMELEN);
        num_matches = 1;
    }
    else if(num_matches > 1){
        int option_num;

        printf("\n");
        for(int i = 0; i < num_matches; i++) {
            printf("%d: %s\n", i+1, candidates[i]);
        }
        printf("Enter the number corresponding to a file (1 to %d): ", num_matches); fflush(stdout);
        scanf("%d", &option_num);
        if(option_num < 1 || option_num > num_matches){
            printf("Out of range error");
            return -1;
        }

        printf("\033[K");
        int cnt = num_matches + 2;
        while(cnt--){
            printf("\033[1A\033[K");
            fflush(stdout);
        }
        printf("\033[1;33mwish> \033[s\033[0m");
        
        strncpy(buf, candidates[option_num-1], MAXFILENAMELEN);
        printf("%s", line_buf); fflush(stdout);
    }
    return num_matches;
}

int main()
{
    Signal(SIGINT, sigint_handler); /* ctrl-c */
    Signal(SIGTSTP, SIG_IGN); /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);

    startShell();

    char c;
    char cmdline[MAXLINE]; /* Command Line */
    for(int i=0;i<MAXLINE;i++) cmdline[i]='\0';
    int cmdline_cnt = 0;
    char prevc = '\0';
    
    while (1)
    {
        /* Prompt */
        printf("\033[1;33mwish> \033[s\033[0m");
        cmdline_cnt = 0;
        for(int i=0;i<MAXLINE;i++){
            cmdline[i]='\0';
        }
        int num_matches = 0;
        int history_displayed = 0;
        while (1) {
            c = getch();
            if(c == KEY_TAB) {
                num_matches = autocomplete(cmdline);
                if(num_matches < 0){
                    cmdline[0] = '\0';
                    cmdline_cnt = 0;
                    break;
                }
                cmdline_cnt = strlen(cmdline);
            }
            else if(c == KEY_CTRL_R) {
                printf("\n");
                searchInHist();
                history_displayed = 1;
                break;
            }
            else if(c == KEY_BACKSPACE) {
                putchar('\b');
                putchar(' ');
                putchar('\b');
                cmdline[--cmdline_cnt] = '\0';
            }
            else if (c == KEY_ENTER) {
                if(prevc == KEY_TAB && (num_matches > 1 || num_matches < 0)) {
                    prevc = c;
                    continue;
                }
                putchar('\n');
                cmdline[cmdline_cnt++] = '\n';
                break;
            }
            else if (c == EOT) {
                putchar('\n');
                stopShell();
            }
            else {
                putchar(c);
                cmdline[cmdline_cnt++] = c;
            }
            prevc = c;
        }

        if(history_displayed == 1){
            continue;
        }

        if(strcmp(cmdline,"quit\n")==0)
            stopShell();
    
        /* Add to history*/
        string command(cmdline);
        
        addToHist(commands,command);
        char* firstSpace;
        watchcmd = 0;
        char* temp = cmdline;
        while(*temp ==' ')
            temp++;
        if(firstSpace = strchr(temp,' '))
        {
            *firstSpace = '\0';
            if(strcmp(temp,"multiWatch")==0)
                watchcmd =1;
            *firstSpace = ' ';
        }
        /* Evaluate if the cmd is non-empty (1 for the newline at the end)*/
        if(strlen(cmdline) > 1){
            if(watchcmd == 0)
                eval(cmdline);
            else 
            {
                flag = 0;
                outputFile = NULL;
                watchCommand wcs[1000];
                int sz = watchParser(cmdline,wcs);
                watcheval(sz,wcs);
            }
        }
    }
    stopShell();
    return 0;
}
/* $end shellmain */

/* $begin eval */
/* eval - Evaluate a command line */

void eval(char *cmdline)
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    
    int background = 0;
    if(char * x = strchr(cmdline,'&'))
        background = 1, *x = ' ';
    
    // Forking to run entire command
    if((pidout = Fork())==0)
    {
        if(!watchcmd)
            setpgid(0, 0);

        if(!background){
            if(!watchcmd){
                Signal(SIGTTOU, SIG_IGN);
                tcsetpgrp(STDIN_FILENO, getpid());
            }
            Signal(SIGINT, SIG_DFL);
            Signal(SIGTSTP, SIG_DFL);
        }

        char* pipeloc;
        int commandInPipe = 0;
        int pipefd[2];
        while(1)
        {
            pipeloc = strchr(cmdline,'|');
            
            pid_t pid;           /* Process id */
            cmdlineProps prop;
            if(commandInPipe)
            {
                /* If already command in pipe,
                * current command shall read from pipefd[0] */
                prop.rfd = pipefd[0];
            }
            if(pipeloc != NULL)
            {
                *pipeloc = '\0';
                strcpy(buf, cmdline);
                strcat(buf,"\n");

                /* Create a pipe, pipefd[1] for write, pipefd[0] for read */
                Pipe(pipefd);
                /* Current command shall write to pipefd[1] */
                prop.wfd = pipefd[1];

                cmdline = pipeloc+1;
                commandInPipe = 1;
            }
            else 
            {
                strcpy(buf, cmdline);
            }
            // printf("Write: %d Read: %d Pipe Write: %d Pipe Read: %d\n", prop.wfd, prop.rfd,pipefd[1], pipefd[0]);
            parseline(buf, argv, prop);
            if (argv[0] == NULL)
            {
                return; /* Ignore empty lines */
            }
        
            if ((pid = Fork()) == 0)
            {   
                /* Input redirection from prop.rfd
                * Close prop.rfd after dup2() as it's no longer needed */
                Dup2(prop.rfd, STDIN_FILENO);
                if(prop.rfd != STDIN_FILENO) Close(prop.rfd);
                
                /* Output redirection from prop.wfd
                * Close prop.wfd after dup2() as it's no longer needed */
                Dup2(prop.wfd, STDOUT_FILENO);
                if(prop.wfd != STDOUT_FILENO) Close(prop.wfd);
               
                
                /* history built-in */
                if(strcmp(argv[0],"history")==0)
                {   
                    displayHist(commands);

                    exit(0);
                }
                /* Child runs user job */
                if (execvp(argv[0], argv) < 0)
                {
                    printf("%s: Command not found.\n", argv[0]);
                    exit(0);
                }
                
            }
            
            int status;
            Waitpid(pid, &status, WUNTRACED);
            if(WIFSTOPPED(status)){
                Kill(pid, SIGCONT);
                Waitpid(pid, &status, 0);
            }
            
            /* Parent must close the pipe endpoints that were used by the child */
            if(prop.rfd != STDIN_FILENO) Close(prop.rfd);
            if(prop.wfd != STDOUT_FILENO) Close(prop.wfd);
                
            if(pipeloc == NULL)
                break;
        }
        // Exitting first level child process   
        exit(0);
    }
    else 
    {
        if(!watchcmd)
            setpgid(pidout, pidout);

        if (!background)
        {
            if(!watchcmd){
                Signal(SIGTTOU, SIG_IGN);
                tcsetpgrp(STDIN_FILENO, pidout);
            }
            int status;
            Waitpid(pidout, &status, WUNTRACED);
            if(WIFSTOPPED(status)){
                Kill(pidout, SIGCONT);
                // Waitpid(pidout, &status, 0);
            }
            if(!watchcmd){
                Signal(SIGTTOU, SIG_IGN);
                tcsetpgrp(STDIN_FILENO, getpid());
            }
        }
        else
        {
            printf("%d %s", pidout, cmdline);
        }
    }
    return;
}

/* $end eval */
/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
// Takes command in the form "<command>\n"
void parseline(char *buf, char **argv, cmdlineProps &prop)
{

    char *delim; /* Points to first space delimiter */
    int argc;    /* Number of args */

    buf[strlen(buf) - 1] = ' ';   /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
        buf++;

    /* Build the argv list */
    argc = 0;
    char* words[MAXARGS];

    while ((delim = strchr(buf, ' ')))
    {
        if(*buf=='\"')
        {
            buf++;
            delim = strchr(buf,'\"');
            *delim = ' ';
        }
        words[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    words[argc] = NULL;
   

    if (argc == 0) /* Ignore blank line */
    {
        argv[0]=NULL;
        return ;
    }

    /* Build the argv list */
    int argvPos = 0;
    for(int wordind = 0; wordind < argc; wordind ++)
    {
        char* word = words[wordind];
        /* Output redirection : '>' and following word skipped for argv */
        if(strcmp(word,">")==0)
        {
            // Opening (creating if not existing) file to write after truncating
            // Permissions : -rw-r--r--
            if(prop.wfd != STDOUT_FILENO) Close(prop.wfd);
            prop.wfd = Open(words[wordind+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            wordind++;
        }
        /* Input redirection : '<' and following word skipped for argv */
        else 
        if(strcmp(word,"<")==0)
        {
            // Opening file to read
            // Permissions : ----------
            if(prop.rfd != STDIN_FILENO) Close(prop.rfd);
            prop.rfd = Open(words[wordind+1], O_RDONLY, 0);
            wordind++;
        }
        else 
        {
           
            argv[argvPos++]=word;
        }
    }
    
    argv[argvPos]=NULL;
    
   

    return;
}

/* $end parseline */

int watchParser(char* buf, watchCommand* wcs )
{
    int n = strlen(buf);
    
    int openQuote = 0;
    int curCommand = -1;
    int curPos = 0;
    char command[MAXLINE];
    char* startofWord;
    int files[n]={0};
    for(int ind = 0; ind < n; ind++)
    {
        if(buf[ind] == '-' && ind+1<n && buf[ind+1]=='t')
            flag = 1;
        if(buf[ind]=='\"' && openQuote ==0)
        {
            curCommand++;
            curPos = 0;
            openQuote = 1;
            startofWord = buf+ind+1;
        }
        else 
        if(buf[ind]=='\"' && openQuote == 1 && (buf[ind+1]==',' || buf[ind+1]==']'))
        {
            cmdlineProps curProp;
            buf[ind]='\0';
            buf[ind+1]='\0';
            wcs[curCommand].argv = startofWord;
            openQuote = 0;
        }
        if(buf[ind] == '>' && openQuote ==0)
        {
            char temp[MAXLINE];
            clear(temp);
            int tempind = 0;
            for(int j = ind+1; buf[j]!='\n'; j++)
            {
                if(buf[j]!=' ')
                    temp[tempind++]=buf[j];
            }
            if(tempind!=0)
            {
                temp[tempind] = '\0';
                outputFile = (char*)malloc((strlen(temp)+1)*sizeof(char));
                strcpy(outputFile,temp);
            }
        }
        
        
    }
    return curCommand+1;
}
void watcheval(int sz, watchCommand* wcs )
{
    int maxfiled = -1;
    vector<string>filenames(sz);
    pids.clear();
    int stdoutcopy;
    stdoutcopy = dup(1);
    if(outputFile != NULL)
    {
        int x = Open(outputFile,O_WRONLY | O_CREAT | O_TRUNC, 0644); 
        Dup2(x,STDOUT_FILENO);
        Close(x);

    }
   for(int commandInd = 0; commandInd<sz; commandInd++)
   {
       pid_t pid;
       
       if( (pid = Fork())==0)
       {
           
           
           Signal(SIGINT, SIG_DFL);
           Signal(SIGTSTP, SIG_DFL);
           char doc[1000];
           
           sprintf(doc,"DocTemp%d",getpid());
           filenames[commandInd] = doc;
           
           sprintf(doc,"%s > DocTemp%d\n",wcs[commandInd].argv,getpid());
           
            int timer = 2;
           while(1)
           {
            eval(doc);
            if(flag==1)
                break;
            sleep(2);
           }
           exit(0);
       }
       else 
       {
           char doc[1000];
           sprintf(doc,"DocTemp%d",pid);
           filenames[commandInd] = doc;
           pids.insert(pid);
       }
      
    }
    int inotfd = inotify_init1(IN_NONBLOCK);
    fd_set rfds;
     const char * arr[sz];
     unordered_map<int,int>watchmap;
     for(int cmdno =0; cmdno < sz; cmdno++)
    {
        arr[cmdno] = filenames[cmdno].c_str();
        Open(arr[cmdno], O_WRONLY | O_CREAT | O_TRUNC, 0644); 
        int watch_desc = inotify_add_watch(inotfd, arr[cmdno], IN_CLOSE_WRITE);
        watchmap[watch_desc] = cmdno;
    }
    running = 1;
    while(running)
    {
        char buffer[BUF_LEN];
        
        int length = read(inotfd,buffer,BUF_LEN);
        
        for(int i=0; i<length;)
        {
            
            struct inotify_event *event = (struct inotify_event *) &buffer[i];

            if((event->mask & IN_CLOSE_WRITE ))
            {
                fstream fileop;
                fileop.open(arr[watchmap[event->wd]],ios::in);
                string curr;
                int index = watchmap[event->wd];
                if(running)
                printf("%s %u\n <-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-\n Output : %d\n ",wcs[index].argv,(unsigned)time(NULL),index+1);
                
                while(getline(fileop,curr))
                {
                    if(running)
                    cout<<curr<<"\n";
                }
                if(running)
                printf("->->->->->->->->->->->->->->->->->->->\n");
            }
            
            i += EVENT_SIZE + event->len;
        }
        
    }
    if(outputFile!=NULL)
    {
        Dup2(stdoutcopy,1);
        Close(stdoutcopy);
    }
    for(auto it:watchmap)
        inotify_rm_watch(inotfd,it.first);
    for(int cmdno =0; cmdno < sz; cmdno++)
    {
        const char * c = filenames[cmdno].c_str();
        remove(c);
    }      
    pids.clear(); 
    return ;
    
   
}

