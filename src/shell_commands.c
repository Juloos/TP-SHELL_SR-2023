#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "shell_commands.h"
#include "jobs.h"

void cmd_stop(int argc, char *args[]) {
    if (argc > 2)
        fprintf(stderr, "%s: too many arguments\n", args[0]);
    else {
        // Default job id is the one of the last background job created
        int job_id = getlastjob();
        pid_t pid = -1;
        if (argc == 2) {
            if (args[1][0] == '%') {
                // If the argument is not just "%", and is a positive integer
                if (strlen(args[1]) > 1 && '0' <= args[1][1] && args[1][1] <= '9')
                    job_id = atoi(args[1] + 1);
                else
                    fprintf(stderr, "%s: invalid job id\n", args[0]);
            } else if ('0' <= args[1][0] && args[1][0] <= '9') // Otherwise it must be a positive integer
                pid = atoi(args[1]);
            else
                fprintf(stderr, "%s: invalid pid\n", args[0]);
        }
        if (pid != -1)
            job_id = getjob(pid);

        int err = 1;
        // If the job exists, stop it
        if (job_id != -1)
            err = stopjob(job_id);

        switch (err) {
            case 2:
                fprintf(stderr, "%s: Job already stopped\n", args[0]);
                break;
            case 1:
                fprintf(stderr, "%s: No such job\n", args[0]);
                break;
            case 0:
            default:
                printf("[%d] %d  Suspended  %s\n", job_id, getjobpgid(job_id), getjobcmd(job_id));
                break;
        }
    }
}

void cmd_fg(int argc, char *args[]) {
    if (argc > 2)
        fprintf(stderr, "%s: too many arguments\n", args[0]);
    else {
        int job_id = getlastjob();
        pid_t pid = -1;
        if (argc == 2) {
            if (args[1][0] == '%') {
                // If the argument is not just "%", and is a positive integer
                if (strlen(args[1]) > 1 && '0' <= args[1][1] && args[1][1] <= '9')
                    job_id = atoi(args[1] + 1);
                else
                    fprintf(stderr, "%s: invalid job id\n", args[0]);
            } else if ('0' <= args[1][0] && args[1][0] <= '9') // Otherwise it must be a positive integer
                pid = atoi(args[1]);
            else
                fprintf(stderr, "%s: invalid pid\n", args[0]);
        }
        if (pid != -1)
            job_id = getjob(pid);

        int err = 1;
        // If the job exists, set it as the foreground job and continue it
        if (job_id != -1) {
            contjob(job_id);
            err = setfg(job_id);
        }


        switch (err) {
            case 2:  // Practically impossible
                fprintf(stderr, "%s: Job already in foreground\n", args[0]);
                break;
            case 1:
                fprintf(stderr, "%s: No such job\n", args[0]);
                break;
            case 0:
            default:
                printf("%s\n", getjobcmd(job_id));
                break;
        }

        waitfgjob();
    }
}

void cmd_bg(int argc, char *args[]) {
    if (argc > 2)
        fprintf(stderr, "%s: too many arguments\n", args[0]);
    else {
        int job_id = getlastjob();
        pid_t pid = -1;
        if (argc == 2) {
            if (args[1][0] == '%') {
                // If the argument is not just "%", and is a positive integer
                if (strlen(args[1]) > 1 && '0' <= args[1][1] && args[1][1] <= '9')
                    job_id = atoi(args[1] + 1);
                else
                    fprintf(stderr, "%s: invalid job id\n", args[0]);
            } else if ('0' <= args[1][0] && args[1][0] <= '9') // Otherwise it must be a positive integer
                pid = atoi(args[1]);
            else
                fprintf(stderr, "%s: invalid pid\n", args[0]);
        }
        if (pid != -1)
            job_id = getjob(pid);

        int err = 1;
        // If the job exists, continue it
        if (job_id != -1)
            err = contjob(job_id);

        switch (err) {
            case 2:
                fprintf(stderr, "%s: Job already in background\n", args[0]);
                break;
            case 1:
                fprintf(stderr, "%s: No such job\n", args[0]);
                break;
            case 0:
            default:
                printf("[%d] %d  Running    %s\n", job_id, getjobpgid(job_id), getjobcmd(job_id));
                break;
        }
    }
}

void cmd_jobs(int argc, char *args[]) {
    if (argc > 1)
        fprintf(stderr, "%s: too many arguments\n", args[0]);
    else
        printjobs();
}

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

    // Command is "exit" or "quit" (not in a function because of freecmd2(l))
    if (strcmp(cmd[0], "exit") == 0 || strcmp(cmd[0], "quit") == 0) {
        if (argc > 2)
            fprintf(stderr, "%s: too many arguments\n", cmd[0]);
        else {
            int code = 0;
            // If there is an argument, use it as exit code, otherwise use 0 by default
            if (argc == 2)
                code = atoi(cmd[1]);  // RED SECURITY ALERT : atoi not safe !!! :)
            freecmd2(l);
            killjobs();
            exit(code);
        }
    }

    // Command is "cd"
    if (strcmp(cmd[0], "cd") == 0) {
        cmd_cd(argc, cmd);
        return 1;
    }

    // Command is "jobs"
    if (strcmp(cmd[0], "jobs") == 0) {
        cmd_jobs(argc, cmd);
        return 1;
    }

    // Command is "fg"
    if (strcmp(cmd[0], "fg") == 0) {
        cmd_fg(argc, cmd);
        return 1;
    }

    // Command is "bg"
    if (strcmp(cmd[0], "bg") == 0) {
        cmd_bg(argc, cmd);
        return 1;
    }

    // Command is "stop"
    if (strcmp(cmd[0], "stop") == 0) {
        cmd_stop(argc, cmd);
        return 1;
    }

    // Ignore comments (for tests purposes)
    if (cmd[0][0] == '#') {
        return 1;
    }

    return 0;
}
