#ifndef TP_SHELL_SR_2023_JOBS_H
#define TP_SHELL_SR_2023_JOBS_H
#include <sys/types.h>
#include "readcmd.h"

void initjobs(void);

int addjob(pid_t *pids, size_t nb_pids);

int deletejobpid(pid_t pid);

void killjobs(void);

void setfg(int job_id);

void waitfgjob(void);

#endif //TP_SHELL_SR_2023_JOBS_H
