/*
 * Copyright (C) 2002, Simon Nieuviarts
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "readcmd.h"
#include "shell_commands.h"
#include "jobs.h"
#include "csapp.h"

// La vie est plus belle avec des couleurs
#define GREEN "\e[1;32m"
#define BLUE "\e[1;34m"
#define RESET "\e[0m"

#define PIPE_READ 0
#define PIPE_WRITE 1


static sigset_t mask_all, prev_mask;
static int shellprint = 1;


// handle_int - SIGINT handler
void handle_int(int sig) {
    // If there is a foreground job, terminate it
    int fg = getfg();
    if (fg != -1)
        termjob(fg);
}

// handle_tstp - SIGTSTP handler
void handle_tstp(int sig) {
    // If there is a foreground job, stop it
    int fg = getfg();
    if (fg == -1)
        return;

    // Use return value of stopjob() to check for errors
    switch (stopjob(fg)) {
        case 2:
            fprintf(stderr, "stop: Job already stopped\n");
            break;
        case 1:
            fprintf(stderr, "stop: No such job\n");
            break;
        case 0:  // Everything went well
        default:
            if (shellprint)
                printf("\n[%d] %d  Suspended  %s\n", fg, getjobpgid(fg), getjobcmd(fg));
            break;
    }
}

// handle_child - SIGCHLD handler
void handle_child(int sig) {
    int olderrno = errno;               // Save errno
    int status;
    pid_t pid;
    // Reaping all terminated children, but managing Stopped and Continued children as well
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        if (WIFSTOPPED(status))
            stopjobpid(pid);            // If the child was stopped, put the job in "Stopped" status
        else if (WIFCONTINUED(status))
            contjobpid(pid);            // If the child was continued, put the job in "Running" status
        else
            deletejobpid(pid);          // Delete the child from the job list
    }
    errno = olderrno;                   // Restore errno
}

/* show_prompt - Prints the command prompt in standard output, with nice colors and stuff :)
 * Arguments : None
 * Return value : None
 */
void show_prompt() {
    char *home = getenv("HOME");
    char hostname[256];                                 // 255 is the max length of a hostname
    gethostname(hostname, 255);

    char *pwd = getcwd(NULL, 0);                        //  -|
    int cmp = (strncmp(pwd, home, strlen(home)) == 0);  //   |
    size_t homelen = strlen(home);                      //   |
    if (cmp) {                                          //   |> Get CWD and manipulate it to display "~"
        pwd += homelen - 1;                             //   |  instead of the home path
        *pwd = '~';                                     //   |
    }                                                   //  -|
    // Show a nice prompt
    printf("%s%s@%s%s:%s%s%s$ ", GREEN, getenv("USER"), hostname, RESET, BLUE, pwd, RESET);
    if (cmp)                                            //  -|
        pwd -= homelen - 1;                             //   |> Restore and free pwd
    free(pwd);                                          //  -|
}

/* exec_cmd() - Fork into child processes that will execute the command line,
 *              with or without I/O redirection, and with or without piped processes
 * Arguments :
 *  - l - A pointer to the Cmdline struct that represents the scanned command line to execute
 * Return value : None
 */
void exec_cmd(Cmdline *l) {
    // Block SIGCHLD
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_mask);

    int pids_len = 1;
    while (l->seq[pids_len] != NULL)
        pids_len++;

    int old_tube[2], new_tube[2];

    pid_t pids[pids_len];
    for (int i = 0; i < pids_len; i++) {
        old_tube[PIPE_READ] = new_tube[PIPE_READ];
        old_tube[PIPE_WRITE] = new_tube[PIPE_WRITE];

        // Create nb_commands - 1 tubes
        if (i + 1 < pids_len)
            pipe(new_tube);

        if ((pids[i] = Fork()) == 0) {
            // Child

            // Input Redirect if first command
            if ((l->in != NULL) && (i == 0)) {
                int fd = Open(l->in, O_RDONLY, 0);
                Dup2(fd, 0);
            }

            // Prepare to read if not first command
            if (i > 0) {
                Close(old_tube[PIPE_WRITE]);
                Dup2(old_tube[PIPE_READ], 0);
            }

            // Prepare to write if not last command
            if (i + 1 < pids_len) {
                Close(new_tube[PIPE_READ]);
                Dup2(new_tube[PIPE_WRITE], 1);
            }

            // Output Redirect if last command
            if ((l->out != NULL) && (i == pids_len - 1)) {
                int fd = Open(l->out, O_CREAT | O_WRONLY, 0644);
                Dup2(fd, 1);
            }

            // No need to keep job list in child process, freeing memory
            freejobs();

            // Unblock all signals
            Sigprocmask(SIG_UNBLOCK, &mask_all, NULL);
            // Make sure all signal handlers are SIG_DFL
            for (int sig = 1; sig < 32; sig++)  // 31 signals total
                if (sig != SIGKILL && sig != SIGSTOP)
                    Signal(sig, SIG_DFL);

            // Exit with success if it is an internal command (thus executed), errors will be printed in standard error
            if (check_internal_commands(l, i) == 1)
                exit(EXIT_SUCCESS);

            // Execute the external command with check for failure
            if (execvp(l->seq[i][0], l->seq[i]) == -1) {
                perror(l->seq[i][0]);
                freecmd2(l);
                exit(EXIT_FAILURE);
            }
        }
        // Parent

        // Make first process in command line the group leader of the brother processes of the command line
        Setpgid(pids[i], pids[0]);

        // Close tube between process i - 1 and i
        if ((pids_len > 1) && (i > 0)) {
            Close(old_tube[PIPE_READ]);
            Close(old_tube[PIPE_WRITE]);
        }
    }
    // Parent
    int job_id = addjob(l->raw, pids, pids_len);
    if (l->bg == 0)
        setfg(job_id);
    else if (shellprint)
        printf("[%d] %d\n", job_id, pids[0]);

    // Unblock SIGCHLD
    Sigprocmask(SIG_SETMASK, &prev_mask, NULL);
}


int main(int argc, char *argv[]) {
    // If argument is provided, read file instead of stdin and disable shell prints
    if (argc > 1) {
        int fd = Open(argv[1], O_RDONLY, 0);
        Dup2(fd, 0);
        shellprint = 0;
    }

    // Disable the shell prints if stdin is not a terminal (aka is a file)
    if (!isatty(0))
        shellprint = 0;

    // Init mask
    Sigfillset(&mask_all);

    // Init job list and signal handlers
    initjobs();
    Signal(SIGCHLD, handle_child);
    Signal(SIGINT, handle_int);
    Signal(SIGTSTP, handle_tstp);

    Cmdline *l;
    while (1) {
        if (shellprint)
            show_prompt();

        l = readcmd();

        // If input stream closed, normal termination
        if (!l) {
            if (shellprint)
                printf("\n");
            killjobs(); // Kill all remaining jobs before exiting, avoids zombies
            exit(0);    // No need to free l before exit, readcmd() already did it
        }

        // Syntax error, read another command
        if (l->err) {
            fprintf(stderr, "synthax error: %s\n", l->err);
            continue;
        }

        // Empty command
        if (!l->seq[0])
            continue;

        // If internal command with no pipe, execute it directly and continue
        if (!l->seq[1] && check_internal_commands(l, 0) == 1)
            continue;

        // Otherwise execute command with child processes
        exec_cmd(l);

        waitfgjob();
    }
}
