/*
 * Copyright (C) 2002, Simon Nieuviarts
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "readcmd.h"
#include "shell_commands.h"
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


void exec_cmd(char **cmd) {
}

void handle_child(int sig) {
    int status;
    pid_t pid;
    do {
        pid = waitpid(-1, &status, WNOHANG | WUNTRACED);
        if (!WIFEXITED(status))
            perror(0);
    } while (pid > 0);
}


int main() {
    Cmdline *l;

    int jobs = 1;  // rough ID-entification of jobs, to be improved later

    char *home = getenv("HOME");
    char hostname[256];  // 255 is the max length of a hostname
    gethostname(hostname, 255);

    while (1) {
        char *pwd = getcwd(NULL, 0);                            //  -|
        int cmp = (strncmp(pwd, home, strlen(home)) == 0);  //   |
        int homelen = strlen(home);                         //   |
        if (cmp) {                                              //   |> Get CWD and manipulate it to display "~"
            pwd += homelen - 1;                                 //   |  instead of the home path
            *pwd = '~';                                         //   |
        }                                                       //  -|
        // Show a nice prompt
        printf("%s%s@%s%s:%s%s%s$ ", GREEN, getenv("USER"), hostname, RESET, BLUE, pwd, RESET);
        if (cmp)                                                //  -|
            pwd -= homelen - 1;                                 //   |> Restore and free pwd
        free(pwd);                                      //  -|


        l = readcmd();

        // If input stream closed, normal termination
        if (!l) {
            printf("\n");
            exit(0);  // No need to free l before exit, readcmd() already did it
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


        // Remove the SIGCHLD handler if command is not in background mode, in case it was set by a previous command
        if (l->bg == 0)
            Signal(SIGCHLD, SIG_DFL);
        // Otherwise set the SIGCHLD handler
        else
            Signal(SIGCHLD, handle_child);


        int tube[2];
        pipe(tube);

        int argc = 0;
        while (l->seq[argc] != NULL)
            argc++;

        int pid[argc];
        for (int i = 0; i < argc; i++) {
            if ((pid[i] = Fork()) == 0) {
                // Child

                // Input Redirect if first command
                if ((l->in != NULL) && (i == 0)) {
                    int fd = Open(l->in, O_RDONLY, 0);
                    Dup2(fd, 0);
                }

                // Output Redirect if last command
                if ((l->out != NULL) && (i == argc - 1)) {
                    int fd = Open(l->out, O_CREAT | O_WRONLY, 0);
                    Dup2(fd, 1);
                }

                // Todo: comment that
                if (argc > 1) {
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
        // Wait for all children to terminate if not in background
        if (l->bg == 0)
            for (int i = 0; i < argc; i++)
                Waitpid(pid[i], NULL, 0);
        else
            printf("[%d] %d\n", jobs++, pid[0]);
    }
}
