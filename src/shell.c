/*
 * Copyright (C) 2002, Simon Nieuviarts
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "readcmd.h"
#include "csapp.h"

typedef struct cmdline Cmdline;


int main() {
    while (1) {
        Cmdline *l;

        printf("shell> ");
        l = readcmd();

        /* If input stream closed, normal termination */
        if (!l) {
            printf("exit\n");
            exit(0);
        }

        if (l->err) {
            /* Syntax error, read another command */
            printf("error: %s\n", l->err);
            continue;
        }

		if (l->in) printf("in: %s\n", l->in);
		if (l->out) printf("out: %s\n", l->out);

		if (l->seq[0] == NULL) {
			continue;
		}

		if (strcmp(l->seq[0][0], "quit") == 0){
			printf("exit\n");
			exit(EXIT_SUCCESS);
		}
		int pid;
		if ((pid = Fork()) == 0) { //Fils
			execvp(l->seq[0][0], l->seq[0]);
			fprintf(stdout, "%s: command not found\n", l->seq[0][0]);
			exit(EXIT_FAILURE);
		}
		//Père
		Waitpid(pid, NULL, 0);
    }
}
