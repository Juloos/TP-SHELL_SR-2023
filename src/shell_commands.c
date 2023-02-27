#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "shell_commands.h"

void cmd_cd(int argc, char *args[]) {
    char *pwd;

    if (argc > 2)
        fprintf(stderr, "%s: too many arguments\n", args[0]);

    // If no arg, go to home and check for chdir error. Otherwise go to given destination and check for chdir error
    else if ((argc == 1 && chdir(getenv("HOME")) == -1) || chdir(args[1]) == -1) {
        // If error is "Bad address" then ignore it (no mem leak, just chdir freaking out with the home path)
        if (errno == 14)
            goto noerrno;
        perror(args[0]);
    }

    noerrno:
    // Update PWD env variable
    pwd = getcwd(NULL, 0);
    setenv("PWD", pwd, 1);
    free(pwd);
}

int check_internal_commands(Cmdline *l, int cmd_index) {
    char **cmd = l->seq[cmd_index];

    int argc = 1;
    while (cmd[argc] != NULL)
        argc++;

    // Command is "exit" or "quit"
    if (strcmp(cmd[0], "exit") == 0 || strcmp(cmd[0], "quit") == 0) {
        if (argc > 2)
            fprintf(stderr, "%s: too many arguments\n", cmd[0]);
        else {
            int code = 0;
            // If there is an argument, use it as exit code, otherwise use 0 by default
            if (argc == 2)
                code = atoi(cmd[1]);  // RED SECURITY ALERT : atoi not safe !!! :)
            freecmd2(l);
            exit(code);
        }
    }

    // Command is "cd"
    if (strcmp(cmd[0], "cd") == 0) {
        cmd_cd(argc, cmd);
        return 1;
    }
 
    // Ignore comments
    if (cmd[0][0] == '#') {
        return 1;
    }

    return 0;
}
