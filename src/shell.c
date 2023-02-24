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


int main() {
    Cmdline *cmd = NULL;

    char * hostname = getenv("HOSTNAME");
    if (hostname == NULL)
        hostname = "localhost";

    while (1) {
        // Afficher un beau prompt
        printf("%s%s@%s%s:%s%s%s$ ", GREEN, getenv("USER"), hostname, RESET, BLUE, getenv("PWD"), RESET);

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


		int tube[2];
		pipe(tube);

		int nb_commands = 0;
		while (cmd->seq[nb_commands] != NULL)
			nb_commands++;

        int pid[nb_commands];
		for (int i = 0; i < nb_commands; i++) {
			if ((pid[i] = Fork()) == 0) {
				// Child

				// Input Redirect if first command
				if ((cmd->in != NULL) && (i == 0)) {
					int fd = Open(cmd->in, O_RDONLY, 0);
					Dup2(fd, 0);
				}

				// Output Redirect if last command
				if ((cmd->out != NULL) && (i == nb_commands - 1)) {
					int fd = Open(cmd->out, O_CREAT | O_WRONLY, 0);
					Dup2(fd, 1);
				}

                // Todo: comment that
				if (nb_commands > 1) {
					if (i == 0) {
						Close(tube[0]);
						Dup2(tube[1], 1);
					}
					else {
						Close(tube[1]);
						Dup2(tube[0], 0);
					}
				}

				// Execute the command and check that it exists
				if (execvp(cmd->seq[i][0], cmd->seq[i]) == -1) {
					perror(cmd->seq[i][0]);
					exit(EXIT_FAILURE);
				}
			}
		}

        // Parent
		Close(tube[0]);
		Close(tube[1]);
		for (int i = 0; i < nb_commands; i++)
        	Waitpid(pid[i], NULL, 0);
    }
}
