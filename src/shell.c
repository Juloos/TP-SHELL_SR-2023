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
    char *pwd;
    char *login = getenv("USER");
    Cmdline *cmd;

    while (1) {
        pwd = getenv("PWD");
        // Afficher un beau prompt
        printf("%s%s%s:%s%s%s$ ", GREEN, login, RESET, BLUE, pwd, RESET);

        cmd = readcmd();

        // If input stream closed, normal termination
        if (!cmd) {
            printf("\n");
            exit(0);
        }

        // Syntax error, read another command
        if (cmd->err) {
            printf("error: %s\n", cmd->err);
            continue;
        }

        // Empty command
        if (!cmd->seq[0])
            continue;

        // Execute internal command if any
        check_internal_commands(cmd);

		int tube[2];
		pipe(tube);

		int nb_commands = 0;
		while (cmd->seq[nb_commands] != NULL) {
			nb_commands++;
		}
		
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
		for (int i = 0; i < nb_commands; i++) {
        	Waitpid(pid[i], NULL, 0);
		}
    }
}
