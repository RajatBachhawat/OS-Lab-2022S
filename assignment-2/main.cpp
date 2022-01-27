/* $begin shellmain */
#include "syscall.h"

pid_t pidout;
int verbose = 0;
vector<string>commands;


struct cmdlineProps 
{   
    int rfd;
    int wfd; 
    cmdlineProps() : rfd(STDIN_FILENO),wfd(STDOUT_FILENO) {}
};

/* Function prototypes */
void eval(char *cmdline);
void parseline(char *buf, char **argv, cmdlineProps &prop);

/* 
 * sigint_handler - SIGINT handler. The kernel sends a SIGINT whenver
 *     the user types ctrl-c at the keyboard. Simply catch and return.
 */
void sigint_handler(int sig) 
{
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
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
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

int main()
{
    Signal(SIGINT, SIG_IGN); /* ctrl-c */
    Signal(SIGTSTP, SIG_IGN); /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);

    char cmdline[MAXLINE]; /* Command line */
    startShell(commands);

    while (1)
    {
        /* Read */
        
        
        printf("\n> ");
        char *rptr;
        if(((rptr = fgets(cmdline, MAXLINE, stdin)) == NULL) && ferror(stdin)){
            app_error("fgets error");
        }

        if (feof(stdin))
            exit(0);
        if(strcmp(cmdline,"quit\n")==0)
            break;
        
        /* Add to history*/
        string command(cmdline);
        
        addToHist(commands,command);
        /* Evaluate */
        eval(cmdline);
    }
    stopShell(commands);
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
        
        char* pipeloc;
        int commandInPipe = 0;
        int pipefd[2];
        
        if(!background){
            Signal(SIGINT, SIG_DFL);
            Signal(SIGTSTP, SIG_DFL);
        }

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
                
                /* Child runs user job */
                if(strcmp(argv[0],"history")==0)
                {
                   
                    displayHist(commands);
                    searchInHist(commands);
                    exit(0);
                }
                if (execvp(argv[0], argv) < 0)
                {
                    printf("%s: Command not found.\n", argv[0]);
                    exit(0);
                }
                
            }
            int status;
            Waitpid(pid, &status, 0);

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
        if (!background)
        {
            int status;
            if (waitpid(pidout, &status, WUNTRACED) < 0)
                unix_error("waitfg: waitpid error");
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
            prop.wfd = Open(words[wordind+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            wordind++;
        }
        /* Input redirection : '<' and following word skipped for argv */
        else 
        if(strcmp(word,"<")==0)
        {
            // Opening file to read
            // Permissions : ----------
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