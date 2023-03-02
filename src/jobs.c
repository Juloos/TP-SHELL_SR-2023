#include "jobs.h"
#include "readcmd.h"
#include "csapp.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct _job {
    int id;            // Job id
    char *cmd;         // Corresponding command line
    int status;        // Current status of the job, 0: Running, 1: Stopped, 2: Done
    time_t starttime;  // Timestamp of the start of the job
    time_t pausetime;  // Timestamp of the last pause of the job
    pid_t *pids;       // Array of pids, the pids of the processus executing the commands in the command line, -1 designate a terminated processus
    size_t nb_pids;    // Number of pids in the array
} Job;

// Linked list structuration of the jobs
typedef struct _joblist {
    struct _joblist *next;
    Job *job;
} JobList;


#define P_TERMINATED (-1)
#define S_RUNNING 0
#define S_STOPPED 1
#define S_DONE 2

static JobList *jobs;
static Job *fg;
static int nb_id_used = 0;
static sigset_t mask_all, prev_mask;  // Used to block signals until ressource is available


JobList *createjoblist(Job *job) {
    JobList *j = (JobList *) malloc(sizeof(JobList));
    j->next = NULL;
    j->job = job;
    return j;
}

Job *createjob(char *cmd, pid_t *pids, size_t nb_pids) {
    Job *job = (Job *) malloc(sizeof(Job));
    job->id = ++nb_id_used;
    job->status = S_RUNNING;
    job->starttime = time(NULL);
    job->pausetime = job->starttime;
    job->nb_pids = nb_pids;
    job->pids = (pid_t *) malloc(sizeof(pid_t) * nb_pids);
    memcpy(job->pids, pids, sizeof(pid_t) * nb_pids);
    job->cmd = (char *) malloc(sizeof(char) * (strlen(cmd) + 1));
    strcpy(job->cmd, cmd);
    return job;
}

void freejob(Job *job) {
    free(job->cmd);
    free(job->pids);
    free(job);
}

int _addjob(char *cmd, pid_t *pids, size_t nb_pids) {
    JobList *j = createjoblist(createjob(cmd, pids, nb_pids));
    j->next = jobs;
    jobs = j;
    return j->job->id;
}

int _pausejob(int job_id) {
    JobList *j = jobs;
    while (j != NULL) {
        if (j->job->id == job_id) {
            if (j->job->status == S_STOPPED || j->job->status == S_DONE)
                return 0;
            for (int i = 0; i < j->job->nb_pids; i++)
                if (j->job->pids[i] != P_TERMINATED)
                    Kill(-j->job->pids[i], SIGTSTP);
            j->job->status = S_STOPPED;
            j->job->pausetime = time(NULL);
            return 1;
        }
        j = j->next;
    }
    return 0;
}

int _resumejob(int job_id) {
    JobList *j = jobs;
    while (j != NULL) {
        if (j->job->id == job_id) {
            if (j->job->status == S_RUNNING || j->job->status == S_DONE)
                return 0;
            for (int i = 0; i < j->job->nb_pids; i++)
                if (j->job->pids[i] != P_TERMINATED)
                    Kill(-j->job->pids[i], SIGCONT);
            j->job->status = S_RUNNING;
            j->job->starttime += time(NULL) - j->job->pausetime;
            return 1;
        }
        j = j->next;
    }
    return 0;
}

int _deletejobpid(pid_t pid) {
    JobList *j = jobs;
    JobList *prev = NULL;
    while (j != NULL) {
        int i;
        for (i = 0; i < j->job->nb_pids; i++) {
            if (j->job->pids[i] == pid) {
                j->job->pids[i] = P_TERMINATED;
                break;
            }
        }
        if (i == j->job->nb_pids)
            goto whilend;

        i = 0;
        while (i < j->job->nb_pids && j->job->pids[i] == P_TERMINATED)
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

void _freejobs() {
    JobList *j = jobs;
    JobList *prev = NULL;
    while (j != NULL) {
        freejob(j->job);
        prev = j;
        j = j->next;
        free(prev);
    }
}

void _killjobs() {
    JobList *j = jobs;
    JobList *prev = NULL;
    while (j != NULL) {
        for (int i = 0; i < j->job->nb_pids; i++) {
            if (j->job->pids[i] == P_TERMINATED)
                continue;
            Kill(-j->job->pids[i], SIGKILL);
            Waitpid(j->job->pids[i], NULL, 0);
        }
        freejob(j->job);
        prev = j;
        j = j->next;
        free(prev);
    }
}

int _setfg(int job_id) {
    JobList *j = jobs;
    while (j != NULL) {
        if (j->job->id == job_id) {
            fg = j->job;
            return 1;
        }
        j = j->next;
    }
    return 0;
}


void initjobs() {
    Sigfillset(&mask_all);
    jobs = NULL;
    fg = NULL;
}

int addjob(char *cmd, pid_t *pids, size_t nb_pids) {
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_mask);
    int res = _addjob(cmd, pids, nb_pids);
    Sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    return res;
}

int pausejob(int job_id) {
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_mask);
    int res = _pausejob(job_id);
    Sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    return res;
}

int resumejob(int job_id) {
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_mask);
    int res = _resumejob(job_id);
    Sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    return res;
}

int deletejobpid(pid_t pid) {
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_mask);
    int res = _deletejobpid(pid);
    Sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    return res;
}

void freejobs() {
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_mask);
    _freejobs();
    Sigprocmask(SIG_SETMASK, &prev_mask, NULL);
}

void killjobs() {
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_mask);
    _killjobs();
    Sigprocmask(SIG_SETMASK, &prev_mask, NULL);
}

int setfg(int job_id) {
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_mask);
    int res = _setfg(job_id);
    Sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    return res;
}

void waitfgjob() {
    while (fg != NULL);
}
