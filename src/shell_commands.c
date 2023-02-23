#include <stdlib.h>
#include <string.h>
#include "shell_commands.h"

void check_internal_commands(Cmdline *cmd) {
    // Check if command is "exit" or "quit"
    if (strcmp(cmd->seq[0][0], "exit") == 0 || strcmp(cmd->seq[0][0], "quit") == 0)
        cmd_exit();
}

void cmd_exit() {
    exit(0);
}