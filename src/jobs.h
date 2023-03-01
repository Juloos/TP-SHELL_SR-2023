#ifndef TP_SHELL_SR_2023_JOBS_H
#define TP_SHELL_SR_2023_JOBS_H
#include <sys/types.h>
#include "readcmd.h"

void initjobs(void);

int addjob(char *cmd, pid_t *pids, size_t nb_pids);

int pausejob(int job_id);

int resumejob(int job_id);

int deletejobpid(pid_t pid);

void freejobs(void);

int killjob(int job_id, int signum);

void killjobs(void);

int setfg(int job_id);

void waitfgjob(void);

#endif //TP_SHELL_SR_2023_JOBS_H
