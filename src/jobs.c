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
    time_t pausetime;  // Timestamp of the last pause of the job, or of its termination
    pid_t *pids;       // Array of pids, the pids of the processes executing the commands in the command line, -1 designate a terminated processes
    size_t nb_pids;    // Number of pids in the array
} Job;

// Linked list structuration of the jobs
typedef struct _joblist {
    struct _joblist *next;
    Job *job;
} JobList;

#define P_ISTERMINATED(pid) (pid < 0)
#define P_TERMINATION(pid) (P_ISTERMINATED(pid) ? pid : -pid)
#define S_RUNNING 0
#define S_STOPPED 1
#define S_DONE 2

static JobList *jobs;
static Job *fg;
static sigset_t mask_all, prev_mask;  // Used to block signals until resource is available


int getnewid() {
    // Source : https://www.geeksforgeeks.org/find-the-smallest-positive-number-missing-from-an-unsorted-array/
    // Complexity : O(n)

    // n is the highest job id
    int n = 1;
    JobList *j;
    for (j = jobs; j != NULL; j = j->next)
        if (j->job->id > n)
            n = j->job->id;

    // To mark the occurrence of elements. 0 is "false" ans 1 is "true"
    char present[n + 1];
    for (int i = 0; i <= n; i++)
        present[i] = 0;

    // Mark the occurrences
    for (j = jobs; j != NULL; j = j->next)
        present[j->job->id] = 1;

    // Find the first element which didn't appear in the original array
    for (int i = 1; i <= n; i++)
        if (present[i] == 0)
            return i;

    // If the original array was of the type {1, 2, 3} in its sorted form
    return n + 1;
}

JobList *createjoblist(Job *job) {
    JobList *j = (JobList *) malloc(sizeof(JobList));
    j->next = NULL;
    j->job = job;
    return j;
}

Job *createjob(char *cmd, pid_t *pids, size_t nb_pids) {
    Job *job = (Job *) malloc(sizeof(Job));
    job->id = getnewid();
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

void removejob(int job_id) {
    JobList *j = jobs;
    JobList *prev = NULL;
    while (j != NULL) {
        if (j->job->id == job_id) {
            if (prev == NULL)
                jobs = j->next;
            else
                prev->next = j->next;
            freejob(j->job);
            free(j);
            return;
        }
        prev = j;
        j = j->next;
    }
}

int _addjob(char *cmd, pid_t *pids, size_t nb_pids) {
    JobList *j = createjoblist(createjob(cmd, pids, nb_pids));
    j->next = jobs;
    jobs = j;
    return j->job->id;
}

int _stopjob(int job_id) {
    JobList *j = jobs;
    while (j != NULL) {
        if (j->job->id == job_id) {
            if (j->job->status == S_STOPPED || j->job->status == S_DONE)
                return 0;
            Kill(-j->job->pids[0], SIGTSTP);
            return 1;
        }
        j = j->next;
    }
    return 0;
}

int _contjob(int job_id) {
    JobList *j = jobs;
    while (j != NULL) {
        if (j->job->id == job_id) {
            if (j->job->status == S_RUNNING || j->job->status == S_DONE)
                return 0;
            Kill(-j->job->pids[0], SIGCONT);
            return 1;
        }
        j = j->next;
    }
    return 0;
}

int _termjob(int job_id) {
    JobList *j = jobs;
    while (j != NULL) {
        if (j->job->id == job_id) {
            if (j->job->status == S_DONE)
                return 0;
            Kill(-j->job->pids[0], SIGTERM);
            return 1;
        }
        j = j->next;
    }
    return 0;
}

int _deletejobpid(pid_t pid) {
    JobList *j = jobs;
    while (j != NULL) {
        int i;
        for (i = 0; i < j->job->nb_pids; i++) {
            if (j->job->pids[i] == pid) {
                j->job->pids[i] = P_TERMINATION(j->job->pids[i]);
                break;
            }
        }
        if (i == j->job->nb_pids)
            goto whilend;

        i = 0;
        while (i < j->job->nb_pids && P_ISTERMINATED(j->job->pids[i]))
            i++;
        if (i == j->job->nb_pids) {           // If all processes of the command have terminated
            if (j->job->status == S_RUNNING)  // If the job was not already stopped
                j->job->pausetime = time(NULL);
            j->job->status = S_DONE;
            if (j->job == fg) {  // If the job was in foreground, we can free it, otherwise keep it for later notification
                removejob(j->job->id);
                fg = NULL;
            }
        }
        return 1;

        whilend:
        j = j->next;
    }
    return 0;
}

int _contjobpid(pid_t pid) {
    JobList *j = jobs;
    while (j != NULL) {
        if (j->job->pids[0] == (pid)) {
            j->job->status = S_RUNNING;
            j->job->starttime += time(NULL) - j->job->pausetime;
            return 1;
        }
        j = j->next;
    }
    return 0;
}

int _stopjobpid(pid_t pid) {
    JobList *j = jobs;
    while (j != NULL) {
        if (j->job->pids[0] == pid) {
            j->job->status = S_STOPPED;
            j->job->pausetime = time(NULL);
            return 1;
        }
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
        Kill(-j->job->pids[0], SIGKILL);
        for (int i = 0; i < j->job->nb_pids; i++)
            if (!P_ISTERMINATED(j->job->pids[i]))
                Waitpid(j->job->pids[i], NULL, 0);
        freejob(j->job);
        prev = j;
        j = j->next;
        free(prev);
    }
}

int _setfg(int job_id) {
    if (fg != NULL)
        return 0;
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

int _getfg() {
    if (fg == NULL)
        return -1;
    return fg->id;
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

int stopjob(int job_id) {
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_mask);
    int res = _stopjob(job_id);
    Sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    return res;
}

int contjob(int job_id) {
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_mask);
    int res = _contjob(job_id);
    Sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    return res;
}

int termjob(int job_id) {
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_mask);
    int res = _termjob(job_id);
    Sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    return res;
}

int deletejobpid(pid_t pid) {
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_mask);
    int res = _deletejobpid(pid);
    Sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    return res;
}

int contjobpid(pid_t pid) {
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_mask);
    int res = _contjobpid(pid);
    Sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    return res;
}

int stopjobpid(pid_t pid) {
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_mask);
    int res = _stopjobpid(pid);
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

int getfg() {
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_mask);
    int res = _getfg();
    Sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    return res;
}

void waitfgjob() {
    while (fg != NULL)
        sleep(1);
}
