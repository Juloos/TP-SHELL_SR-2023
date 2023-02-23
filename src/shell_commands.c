#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "shell_commands.h"

void cmd_exit(int argc, char *args[]) {
    if (argc > 2)
        fprintf(stderr, "%s: too many arguments\n", args[0]);
    else if (argc == 2)
        // RED SECURITY ALERT : atoi not safe !!!!!!!!!!!!! :)
        exit(atoi(args[1]));
    else
        exit(0);
}

void cmd_cd(int argc, char *args[]) {
    if (argc > 2) {
        fprintf(stderr, "%s: too many arguments\n", args[0]);
        return;
    }
    // If no arg, go to home and check for chdir error. Otherwise go to given destination and check for chdir error
    else if ((argc == 1 && chdir(getenv("HOME")) == -1) || chdir(args[1]) == -1) {
        perror(args[0]);
        return;
    }
    // Update PWD env variable if everything went well
    char *pwd = getcwd(NULL, 0);
    setenv("PWD", pwd, 1);
    free(pwd);
}

int check_internal_commands(char **cmd) {
    int argc = 1;
    while (cmd[argc] != NULL)
        argc++;

    // Command is "exit" or "quit"
    if (strcmp(cmd[0], "exit") == 0 || strcmp(cmd[0], "quit") == 0) {
        cmd_exit(argc, cmd);
        return 1;
    }

    // Command is "cd"
    if (strcmp(cmd[0], "cd") == 0) {
        cmd_cd(argc, cmd);
        return 1;
    }

    return 0;
}
