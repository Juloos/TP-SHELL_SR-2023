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


void handle_child(int sig) {
    int olderrno = errno;                                            // Save errno
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {  // Reaping all terminated children
        if (!WIFEXITED(status))                                      // Child was not terminated normally
            perror(0);
        deletejobpid(pid);                                           // Delete the child from the job list
    }
    errno = olderrno;                                                // Restore errno
}


int main() {
    Cmdline *l;

    sigset_t mask_one, prev_one;
    Sigemptyset(&mask_one);
    Sigaddset(&mask_one, SIGCHLD);

    initjobs();
    Signal(SIGCHLD, handle_child);

    char *home = getenv("HOME");
    static char hostname[256];  // 255 is the max length of a hostname
    gethostname(hostname, 255);

    while (1) {
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


        l = readcmd();

        // If input stream closed, normal termination
        if (!l) {
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

        // Execute internal command and continue if it is one
        if (check_internal_commands(l, 0) == 1)
            continue;


        // * Block SIGCHLD
        Sigprocmask(SIG_BLOCK, &mask_one, &prev_one);

        int tube[2];
        pipe(tube);

        int pids_len = 1;
        while (l->seq[pids_len] != NULL)
            pids_len++;

        pid_t pids[pids_len];
        for (int i = 0; i < pids_len; i++) {
            if ((pids[i] = Fork()) == 0) {
                // Child

                // No need to keep job list in child process, freeing memory
                freejobs();

                // Input Redirect if first command
                if ((l->in != NULL) && (i == 0)) {
                    int fd = Open(l->in, O_RDONLY, 0);
                    Dup2(fd, 0);
                }

                // Output Redirect if last command
                if ((l->out != NULL) && (i == pids_len - 1)) {
                    int fd = Open(l->out, O_CREAT | O_WRONLY, 0);
                    Dup2(fd, 1);
                }

                // Todo: comment that
                if (pids_len > 1) {
                    if (i == 0) {
                        Close(tube[0]);
                        Dup2(tube[1], 1);
                    } else {
                        Close(tube[1]);
                        Dup2(tube[0], 0);
                    }
                }

                // Make it a process group leader
                Setpgid(getpid(), getpid());

                // * Unblock SIGCHLD
                Sigprocmask(SIG_SETMASK, &prev_one, NULL);

                // Execute the command and check that it exists
                if (execvp(l->seq[i][0], l->seq[i]) == -1) {
                    perror(l->seq[i][0]);
                    freecmd2(l);
                    exit(EXIT_FAILURE);
                }
            }
        }
        // Parent

        Close(tube[0]);
        Close(tube[1]);

        int job_id = addjob(l->raw, pids, pids_len);
        if (l->bg == 0)
            setfg(job_id);
        else
            printf("[%d] %d\n", job_id, pids[0]);

        // * Unblock SIGCHLD
        Sigprocmask(SIG_SETMASK, &prev_one, NULL);

        waitfgjob();
    }
}
