#ifndef TP_SHELL_SR_2023_JOBS_H
#define TP_SHELL_SR_2023_JOBS_H
#include <sys/types.h>
#include "readcmd.h"

void initjobs(void);

int addjob(char *cmd, pid_t *pids, size_t nb_pids);

int stopjob(int job_id);

int contjob(int job_id);

int termjob(int job_id);

int deletejobpid(pid_t pid);

void freejobs(void);

void killjobs(void);

int setfg(int job_id);

int getfg(void);

void waitfgjob(void);

#endif //TP_SHELL_SR_2023_JOBS_H
