#include "csapp.h"
#include "tsh_helper.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * If DEBUG is defined, enable contracts and printing on dbg_printf.
 */
#ifdef DEBUG
/* When debugging is enabled, these form aliases to useful functions */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_requires(...) assert(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#define dbg_ensures(...) assert(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated for these */
#define dbg_printf(...)
#define dbg_requires(...)
#define dbg_assert(...)
#define dbg_ensures(...)
#endif

/* Function prototypes */
void eval(const char *cmdline);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);
void sigquit_handler(int sig);
void cleanup(void);

/**
 * 
 *
 * 
 */
int main(int argc, char **argv) {
    char c;
    char cmdline[MAXLINE_TSH]; // Cmdline for fgets
    bool emit_prompt = true;   // Emit prompt (default)

    // Redirect stderr to stdout (so that driver will get all output
    // on the pipe connected to stdout)
    if (dup2(STDOUT_FILENO, STDERR_FILENO) < 0) {
        perror("dup2 error");
        exit(1);
    }

    // Parse the command line
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h': // Prints help message
            usage();
            break;
        case 'v': // Emits additional diagnostic info
            verbose = true;
            break;
        case 'p': // Disables prompt printing
            emit_prompt = false;
            break;
        default:
            usage();
        }
    }

    // Create environment variable
    if (putenv("MY_ENV=42") < 0) {
        perror("putenv error");
        exit(1);
    }

    // Set buffering mode of stdout to line buffering.
    // This prevents lines from being printed in the wrong order.
    if (setvbuf(stdout, NULL, _IOLBF, 0) < 0) {
        perror("setvbuf error");
        exit(1);
    }

    // Initialize the job list
    init_job_list();

    // Register a function to clean up the job list on program termination.
    // The function may not run in the case of abnormal termination (e.g. when
    // using exit or terminating due to a signal handler), so in those cases,
    // we trust that the OS will clean up any remaining resources.
    if (atexit(cleanup) < 0) {
        perror("atexit error");
        exit(1);
    }

    // Install the signal handlers
    Signal(SIGINT, sigint_handler);   // Handles Ctrl-C
    Signal(SIGTSTP, sigtstp_handler); // Handles Ctrl-Z
    Signal(SIGCHLD, sigchld_handler); // Handles terminated or stopped child

    Signal(SIGTTIN, SIG_IGN);
    Signal(SIGTTOU, SIG_IGN);

    Signal(SIGQUIT, sigquit_handler);

    // Execute the shell's read/eval loop
    while (true) {
        if (emit_prompt) {
            printf("%s", prompt);

            // We must flush stdout since we are not printing a full line.
            fflush(stdout);
        }

        if ((fgets(cmdline, MAXLINE_TSH, stdin) == NULL) && ferror(stdin)) {
            perror("fgets error");
            exit(1);
        }

        if (feof(stdin)) {
            // End of file (Ctrl-D)
            printf("\n");
            return 0;
        }

        // Remove any trailing newline
        char *newline = strchr(cmdline, '\n');
        if (newline != NULL) {
            *newline = '\0';
        }

        // Evaluate the command line
        eval(cmdline);
    }

    return -1; // control never reaches here
}


// block /wait unitl the process pig is no longer the foreground process
void wait_fg(pid_t pig)
{
    sigset_t mask;
    sigemptyset(&mask);
    while (fg_job())
    {
        sigsuspend(&mask);
    }
    return;
}

/*
constraint: argv[0] must be one of the builtin commands bg, fg or jobs.
pre: argument list, argv(char**)
a built-in command is like bg jobs/fg jobs
post: do the built-in commant execution

*/



void do_builtin(int argc, char** argv)
{
    char *prefix = argv[1];
    char *command = argv[0];
    sigset_t mask_all, prev_all; // used for blocking/unblocking signals
    pid_t pid = 0;
    jid_t jid = 0;
    //int olderrno = errno;
    sigfillset(&mask_all);

    if (prefix == NULL)
    {
        sio_printf("command line argument needs job id or process id.\n");
        return;
    }
    if (argc != 2)
    {
        sio_printf("command line argument number is wrong.\n");
        return;
    }
    //if it is a pid
    else if ((prefix[0] != '%') && (isdigit(prefix[0]) == true))
    {
        //find the corresponding job id
        pid = (pid_t)atoi(prefix);
        sigprocmask(SIG_BLOCK, &mask_all, &prev_all); //block all signals
        jid = job_from_pid(pid);
        if (job_exists(jid) == false) //if job does not exist
        {
            sio_printf("Can't find the process (%d)", pid);
            sigprocmask(SIG_SETMASK, &prev_all, NULL); //remove the block
            return;
        }
    }
    // if it is a jid
    else if ((prefix[0] == '%') && (isdigit(prefix[1]) == true))
    {
        jid = (jid_t)atoi(&prefix[1]);
        sigprocmask(SIG_BLOCK, &mask_all, &prev_all); //block all signals
        if (job_exists(jid) == false)
        {
            sio_printf("Can't find the process (%d)", pid);
            sigprocmask(SIG_SETMASK, &prev_all, NULL); //remove the block
            return;
        }
        else
        {
            //find a process to send continuous signal
            pid = job_get_pid(jid);
        }

    }
    // if job exists, all signals now are blocked
    // we have found jid, pid of desired job
    // we check if it is a bg job or a fg job by comparing command
    
    if (strcmp(command, "bg") == 0)
    {
        //first check state
        if (job_get_state(jid) == ST)
        {
            job_set_state(jid, BG);
            kill(-pid, SIGCONT);
            sio_printf("[%d] (%d) %s", jid, pid, job_get_cmdline(jid));

        }
        // ? can we run a currely foreground job in background by builtin command
        /*else if ((job_get_state(jid) == FG) || (job_get_state(jid) == UNDEF))
        {
            sio_eprintf("job [%d] state is %s, cannot run background job by command", jid, job_get_state(jid));
        }*/

    }
    else if (strcmp(command, "fg") == 0)
    {
        //first check state
        if (job_get_state(jid) == ST)
        {
            job_set_state(jid, FG);
            kill(-pid, SIGCONT);
            //sigsuspend
            wait_fg(pid);
            sio_printf("[%d] (%d) %s", jid, pid, job_get_cmdline(jid));
            


        }
        // ? can we run a currely foreground job in background by builtin command
        /*else if ((job_get_state(jid) == BG) || (job_get_state(jid) == UNDEF))
        {
            sio_eprintf("job [%d] state is %s, cannot run background job by command", jid, job_get_state(jid));
        }*/
    }
    sigprocmask(SIG_SETMASK, &prev_all, NULL); //remove the block
    //errno = olderrno;
    return;

}
















//pre: argument list argv
// post: return true if argv[0] is one of the "bg", "fg", "job" and exit immediately if argv[0] is "quit"
// else: return false

bool is_builtin(int argc, char ** argv)
{
    sigset_t mask_all, prev_all;
    sigfillset(&mask_all);
    
    if (strcmp(argv[0], "quit") == 0) //quit command
    {
        exit(0);
    }
    else if (strcmp(argv[0], "&") == 0) //ignore singleton
    {
        return true;
    }
    else if (strcmp(argv[0], "jobs") == 0) //list all background jobs
    {
        int out_fid = STDOUT_FILENO;
        // need to block signals
        sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
        list_jobs(out_fid);
        sigprocmask(SIG_SETMASK, &prev_all, NULL); //remove the block
        return true;
    }
    else if (strcmp(argv[0], "fg") == 0) //  built-in command  run job in foreground
    {
        do_builtin(argc, argv);
        return true;

    }
    else if (strcmp(argv[0], "bg") == 0) //  built-in command  run job in background
    {
        do_builtin(argc, argv);
        return true;

    }
    return false;
    
}

















/**
 * @brief <What does eval do?>
 *
 * TODO: 
 * pre: command line(string)
 * post :  parses, interprets(detect if builtpin command), and executes the command line. 
 * 
 *
 * NOTE: The shell is supposed to be a long-running process, so this function
 *       (and its helpers) should avoid exiting on error.  This is not to say
 *       they shouldn't detect and print (or otherwise handle) errors!
 */
void eval(const char *cmdline) {
    parseline_return parse_result;
    struct cmdline_tokens token;
    pid_t pid;
    jid_t jid;
    sigset_t mask, mask_prev, mask_all;
    job_state state = UNDEF;
    sigfillset(&mask_all);
    int argc;
    char **argv;
    //int olderrno= errno;
    // Parse command line
    // return argv,argc in the structure of token
    /*parse_result has 4 possibilities:  
    PARSELINE_FG = 4,    ///< Foreground job -->check if built-in
    PARSELINE_BG = 5,    ///< Background job --> check if built-in
    PARSELINE_EMPTY = 6, ///< Empty cmdline  immediately return 
    PARSELINE_ERROR = 7, ///< Parse error immediately return 
    */
    parse_result = parseline(cmdline, &token);
    argc = token.argc;
    argv = token.argv;

    // print message if error in parse line or empty line, return immediately
    if (parse_result == PARSELINE_ERROR || parse_result == PARSELINE_EMPTY) {
        return;
    }
    else{
        // check if argv[0] is one of the four built-in command
        // call is_builtin function
        // this code skeleton is from our text book page 757
        if (parse_result == PARSELINE_FG)
        {
            state = FG;
        }
        else if (parse_result == PARSELINE_BG)
        {
            state = BG;
        }
        if (is_builtin(argc, argv) == false)
        {
            //blocking signals
            sigemptyset(&mask);
            sigaddset(&mask, SIGCHLD);
            sigaddset(&mask, SIGINT);
            sigaddset(&mask, SIGTSTP);
            sigprocmask(SIG_BLOCK, &mask, &mask_prev);
            // child process
            if ((pid = fork()) == 0)
            {
                //unblock sigchild,sigint, sigstp signal
                sigprocmask(SIG_SETMASK, &mask_prev, NULL);
                //reset job group
                setpgid(0,0);

                //execute file
                if (execve(argv[0], argv, environ) < 0)
                {
                    printf("%s: command not found\n", argv[0]);
                    exit(0);
                }

            }
            //parent process -->first add job
            //add job-->need to block all signals, otherwise may casue error

            sigprocmask(SIG_BLOCK, &mask_all, NULL); 
            // need block SIGCHILD?
            if (state == FG)
            {
                add_job(pid, state, cmdline);
                wait_fg(pid);
                //sigprocmask(SIG_SETMASK, &mask_prev, &NULL);
            }
            //print out background message
            // need to block all signals
            else if (state == BG)
            {
                add_job(pid, state, cmdline);
                jid = job_from_pid(pid);
                
                sio_printf("[%d] (%d) %s\n", jid, (int) pid, cmdline);
            }
            //unblocking signals
            sigprocmask(SIG_SETMASK, &mask_prev, NULL);



        }







    }
    //errno = olderrno;
}




/*****************
 * Signal handlers
 *****************/

/**
 * @brief <What does sigchld_handler do?>
 *
 * TODO: this function is called when the main process/shell receives SIGCHILD
 * when child process terminates and becomes a zombie, or when child process stop/suspend due to the SIGSTOP/SIGSTP signal sent by kernel
 * which can be resumed by sending SIGCONT. Note: SIGSTOP cannot be blocked
 * The most import job for this function is reaping the zombie child, while not waiting for the currently running child
 */
void sigchld_handler(int sig) 
{
    pid_t pid;
    int state;
    int olderrno = errno;
    sigset_t mask_all, prev_all;
    sigfillset(&mask_all);
    // use while loop to reape all terminated child
    while ((pid = waitpid(-1, &state, WNOHANG | WUNTRACED)) > 0)
    {
        //block the signal
        sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
        //find the corresponding job id
        jid_t jid = job_from_pid(pid);
        // check the child status
        if (WIFEXITED(state)) //exit normally
        {
            //sio_printf("child %d terminated normally with exit status=%d\n", pid, WEXITSTATUS(state));
            delete_job(jid);
        }
        //terminated by some uncatched signal
        else if (WIFSIGNALED(state))
        {
            sio_printf("Job [%d] (%d) terminated by signal %d\n", jid, (int) pid, WTERMSIG(state));
            delete_job(jid);
        }
        // stop/pause by signal( don't delete from job list)
        else if (WIFSTOPPED(state))
        {
            job_set_state(jid, ST);
            sio_printf("Job [%d] (%d) stopped by signal %d\n", jid, (int) pid, WSTOPSIG(state));
            //sio_printf("hello world\n");
        }
        //sigprocmask(SIG_SETMASK, &prev_all, NULL); //remove the block
    }
    /*if (errno != ECHILD)
    {
        sio_eprintf("waitpid error\n");
    }*/
    sigprocmask(SIG_SETMASK, &prev_all, NULL); //remove the block
    errno = olderrno;
}

/**
 * @brief <What does sigint_handler do?>
 *
 * TODO: Handles SIGINT signals (sent by Ctrl-C).
 * catch the signal(SIGINT) and forward it to the entire process group that contains the foreground
job. If there is no foreground job, then these signals should have no effect.
 */
void sigint_handler(int sig) 
{
    int olderrno = errno;
    sigset_t mask_all, prev_all;
    sigfillset(&mask_all);
    //block the signal
    sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
    // first need to find if there is forground job running/in the job list.
    jid_t cur_fg = fg_job();
    if ((int)(cur_fg) == 0)
    {
        sigprocmask(SIG_SETMASK, &prev_all, NULL); //remove the block
        errno = olderrno;
        return;
    }
    else
    {
        pid_t cur_fg_pid = job_get_pid(cur_fg);
        //sigprocmask(SIG_SETMASK, &prev_all, NULL); //remove the block
        if ((int)cur_fg_pid != 0)
        {
            //kill(-cur_fg_pid, sig);
            kill(-cur_fg_pid, SIGINT);
        }
        //sigprocmask(SIG_SETMASK, &prev_all, NULL); //remove the block
    }
    sigprocmask(SIG_SETMASK, &prev_all, NULL); //remove the block
    //restore the errono
    errno = olderrno;

}

/**
 * @brief <What does sigtstp_handler do?>
 *
 * TODO: similar to sigint_handler, suspend the foreground job when receives the signal sig
 * catch the signal(SIGSTP) and forward it to the entire process group that contains the foreground
job. If there is no foreground job, then these signals should have no effect.
 */
void sigtstp_handler(int sig) 
{
   
    int olderrno = errno;
    sigset_t mask_all, prev_all;
    sigfillset(&mask_all);
    //block the signal
    sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
    // first need to find if there is forground job running/in the job list.
    jid_t cur_fg = fg_job();
    if ((int)(cur_fg) == 0)
    {
        sigprocmask(SIG_SETMASK, &prev_all, NULL); //remove the block
        errno = olderrno;
        return;
    }
    else
    {
        pid_t cur_fg_pid = job_get_pid(cur_fg);
        if ((int)cur_fg_pid != 0)
        {
            //kill(-cur_fg_pid, sig);
            kill(-cur_fg_pid, SIGTSTP);
        }
        //sigprocmask(SIG_SETMASK, &prev_all, NULL); //remove the block
    }
    sigprocmask(SIG_SETMASK, &prev_all, NULL); //remove the block
    //restore the errono
    errno = olderrno;
}

/**
 * @brief Attempt to clean up global resources when the program exits.
 *
 * In particular, the job list must be freed at this time, since it may
 * contain leftover buffers from existing or even deleted jobs.
 */
void cleanup(void) {
    // Signals handlers need to be removed before destroying the joblist
    Signal(SIGINT, SIG_DFL);  // Handles Ctrl-C
    Signal(SIGTSTP, SIG_DFL); // Handles Ctrl-Z
    Signal(SIGCHLD, SIG_DFL); // Handles terminated or stopped child

    destroy_job_list();
}
