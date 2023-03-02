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
#define BLACK "\e[1;30m"
#define RED "\e[1;31m"
#define GREEN "\e[1;32m"
#define YELLOW "\e[1;33m"
#define BLUE "\e[1;34m"
#define MAGENTA "\e[1;35m"
#define CYAN "\e[1;36m"
#define WHITE "\e[1;37m"
#define RESET "\e[0m"

#define PIPE_READ 0
#define PIPE_WRITE 1


static sigset_t mask_all, prev_mask;
static int shellprint = 1;


void handle_int(int sig) {
    // If there is a foreground job, terminate it
    int fg = getfg();
    if (fg != -1)
        termjob(fg);
}

void handle_tstp(int sig) {
    // If there is a foreground job, stop it
    int fg = getfg();
    if (fg != -1)
        stopjob(fg);
}

void handle_child(int sig) {
    int olderrno = errno;                                                         // Save errno
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {  // Reaping all terminated children
        if (WIFSTOPPED(status))
            stopjobpid(pid);                                                      // If the child was stopped, stop the job
        else if (WIFCONTINUED(status))
            contjobpid(pid);                                                      // If the child was continued, continue the job
        else
            deletejobpid(pid);                                                    // Delete the child from the job list
    }
    errno = olderrno;                                                             // Restore errno
}

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

        // If internal command (with no pipe), execute it directly and continue
        if (!l->seq[1] && check_internal_commands(l, 0) == 1)
            continue;

        // Otherwise execute command with child processes
        exec_cmd(l);

        waitfgjob();
    }
}
