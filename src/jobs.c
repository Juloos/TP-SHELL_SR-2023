#include "jobs.h"
#include "csapp.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct _job {
    int id;
    pid_t *pids;
    int nb_pids;
} Job;

typedef struct _joblist {
    struct _joblist *next;
    Job *job;
} JobList;


const pid_t TERMINATED_PID = -1;

static JobList *jobs;
static Job *fg;
static int nb_id_used = 0;
sigset_t mask_all, prev_all;  // * Used to block signals until ressource is available


JobList *createjoblist(Job *job) {
    JobList *j = (JobList *) malloc(sizeof(JobList));
    j->next = NULL;
    j->job = job;
    return j;
}

Job *createjob(pid_t *pids, size_t nb_pids) {
    Job *job = (Job *) malloc(sizeof(Job));
    job->id = ++nb_id_used;
    job->nb_pids = nb_pids;
    job->pids = (pid_t *) malloc(sizeof(pid_t) * nb_pids);
    memcpy(job->pids, pids, sizeof(pid_t) * nb_pids);
    return job;
}

void freejob(Job *job) {
    free(job->pids);
    free(job);
}

int _addjob(pid_t *pids, size_t nb_pids) {
    JobList *j = createjoblist(createjob(pids, nb_pids));
    j->next = jobs;
    jobs = j;
    return j->job->id;
}

int _deletejobpid(pid_t pid) {
    JobList *j = jobs;
    JobList *prev = NULL;
    while (j != NULL) {
        int i;
        for (i = 0 ; i < j->job->nb_pids ; i++) {
            if (j->job->pids[i] == pid) {
                j->job->pids[i] = TERMINATED_PID;
                break;
            }
        }
        if (i == j->job->nb_pids)
            goto whilend;

        i = 0;
        while (i < j->job->nb_pids && j->job->pids[i] == TERMINATED_PID)
            i++;
        if (i == j->job->nb_pids) {  // All processes of the command terminated
            if (prev == NULL)
                jobs = j->next;
            else
                prev->next = j->next;
            if (fg == j->job)
                fg = NULL;
            freejob(j->job);
            free(j);
        }
        return 1;

        whilend:
        prev = j;
        j = j->next;
    }
    return 0;
}

void _killjobs() {
    JobList *j = jobs;
    JobList *prev = NULL;
    while (j != NULL) {
        for (int i = 0; i < j->job->nb_pids; i++) {
            Kill(j->job->pids[i], SIGKILL);
            Waitpid(j->job->pids[i], NULL, 0);
        }
        freejob(j->job);
        prev = j;
        j = j->next;
        free(prev);
    }
}

void _setfg(int job_id) {
    JobList *j = jobs;
    while (j != NULL) {
        if (j->job->id == job_id) {
            fg = j->job;
            return;
        }
        j = j->next;
    }
}


void initjobs() {
    Sigfillset(&mask_all);
    jobs = NULL;
    fg = NULL;
}

int addjob(pid_t *pids, size_t nb_pids) {
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
    int res = _addjob(pids, nb_pids);
    Sigprocmask(SIG_SETMASK, &prev_all, NULL);
    return res;
}

int deletejobpid(pid_t pid) {
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
    int res = _deletejobpid(pid);
    Sigprocmask(SIG_SETMASK, &prev_all, NULL);
    return res;
}

void killjobs() {
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
    _killjobs();
    Sigprocmask(SIG_SETMASK, &prev_all, NULL);
}

void setfg(int job_id) {
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
    _setfg(job_id);
    Sigprocmask(SIG_SETMASK, &prev_all, NULL);
}

void waitfgjob() {
    while (fg != NULL);
}
