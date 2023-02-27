#ifndef TP_SHELL_SR_2023_JOBS_H
#define TP_SHELL_SR_2023_JOBS_H
#include <sys/types.h>
#include "readcmd.h"

void initjobs(void);

int addjob(Cmdline *l, pid_t *pids);

int deletejob(int job_id);

void killjobs(void);

void waitfgjob(void);

#endif //TP_SHELL_SR_2023_JOBS_H
