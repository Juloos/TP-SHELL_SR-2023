#ifndef TP_SHELL_SR_2023_JOBS_H
#define TP_SHELL_SR_2023_JOBS_H
#include <sys/types.h>

void initjobs(void);

int addjob(pid_t pid);

void deletejob(pid_t pid);

void killjobs(void);

#endif //TP_SHELL_SR_2023_JOBS_H
