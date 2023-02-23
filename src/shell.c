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
#define BLACK "\e[0;30m"
#define RED "\e[0;31m"
#define GREEN "\e[0;32m"
#define YELLOW "\e[0;33m"
#define BLUE "\e[0;34m"
#define MAGENTA "\e[0;35m"
#define CYAN "\e[0;36m"
#define WHITE "\e[0;37m"
#define RESET "\e[0m"


void exec_cmd(char **cmd) {
}


int main() {
    Cmdline *cmd = NULL;

    while (1) {
        // Afficher un beau prompt
        printf("%s%s%s:%s%s%s$ ", GREEN, getenv("USER"), RESET, BLUE, getenv("PWD"), RESET);

        cmd = readcmd();

        // If input stream closed, normal termination
        if (!cmd) {
            printf("\n");
            exit(0);
        }

        // Syntax error, read another command
        if (cmd->err) {
            fprintf(stderr, "synthax error: %s\n", cmd->err);
            continue;
        }

        // Empty command
        if (!cmd->seq[0])
            continue;

        // Execute internal command and continue if any
        if (check_internal_commands(cmd->seq[0]) == 1)
            continue;

        int pid;
        if ((pid = Fork()) == 0) {
            // Child
            // Execute the command and check that it exists
            if (execvp(cmd->seq[0][0], cmd->seq[0]) == -1) {
                perror(cmd->seq[0][0]);
                exit(EXIT_FAILURE);
            }
        }
        // Parent
        Waitpid(pid, NULL, 0);
    }
}
