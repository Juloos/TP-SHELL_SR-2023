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
    pid_t *pids;       // Array of pids, the pids of the processes executing the commands in the command line, a negative pid designate a terminated processes
    size_t nb_pids;    // Number of pids in the array
} Job;

// Linked list structuration of the jobs
typedef struct _joblist {
    struct _joblist *next;
    Job *job;
} JobList;

#define P_PID(pid) (pid < 0 ? -pid : pid)
#define P_PGID(pid) (pid < 0 ? pid : -pid)
#define P_ISTERMINATED(pid) (pid < 0)
#define P_TERMINATE(pid) P_PGID(pid)
#define S_RUNNING 0
#define S_STOPPED 1
#define S_DONE 2

static JobList *jobs;
static Job *fg;
static sigset_t mask_all, prev_mask;  // Used to block signals until resource is available


static int getnewid() {
    // Source : https://www.geeksforgeeks.org/find-the-smallest-positive-number-missing-from-an-unsorted-array/
    // Complexity : O(n)

    // n is the highest job id
    int n = 1;
    JobList *jl;
    for (jl = jobs; jl != NULL; jl = jl->next)
        if (jl->job->id > n)
            n = jl->job->id;

    // To mark the occurrence of elements. 0 is "false" ans 1 is "true"
    char present[n + 1];
    for (int i = 0; i <= n; i++)
        present[i] = 0;

    // Mark the occurrences
    for (jl = jobs; jl != NULL; jl = jl->next)
        present[jl->job->id] = 1;

    // Find the first element which didn't appear in the original array
    for (int i = 1; i <= n; i++)
        if (present[i] == 0)
            return i;

    // If the original array was of the type {1, 2, 3} in its sorted form
    return n + 1;
}

static JobList *createjoblist(Job *job) {
    JobList *jl = (JobList *) malloc(sizeof(JobList));
    jl->next = NULL;
    jl->job = job;
    return jl;
}

static Job *createjob(char *cmd, pid_t *pids, size_t nb_pids) {
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

static void freejob(Job *job) {
    free(job->cmd);
    free(job->pids);
    free(job);
}

static void removejob(int job_id) {
    JobList *jl = jobs;
    JobList *prev = NULL;
    while (jl != NULL) {
        if (jl->job->id == job_id) {
            if (prev == NULL)
                jobs = jl->next;
            else
                prev->next = jl->next;
            freejob(jl->job);
            free(jl);
            return;
        }
        prev = jl;
        jl = jl->next;
    }
}

static Job *findjob(int job_id) {
    JobList *jl = jobs;
    while (jl != NULL) {
        if (jl->job->id == job_id)
            return jl->job;
        jl = jl->next;
    }
    return NULL;
}

static Job *pidfindjob(pid_t pid) {
    JobList *jl = jobs;
    while (jl != NULL) {
        for (size_t i = 0; i < jl->job->nb_pids; i++)
            if (P_PID(jl->job->pids[i]) == pid)
                return jl->job;
        jl = jl->next;
    }
    return NULL;
}

static Job *pgidfindjob(pid_t pgid) {
    // Hypothesis : pgid is the pid of the first process of the job
    JobList *jl = jobs;
    while (jl != NULL) {
        if (P_PID(jl->job->pids[0]) == pgid)
            return jl->job;
        jl = jl->next;
    }
    return NULL;
}

static int _addjob(char *cmd, pid_t *pids, size_t nb_pids) {
    JobList *jl = createjoblist(createjob(cmd, pids, nb_pids));
    jl->next = jobs;
    jobs = jl;
    return jl->job->id;
}

static int _stopjob(int job_id) {
    Job *job = findjob(job_id);
    if (job == NULL)
        return 1;  // Job not found
    else if (job->status == S_STOPPED || job->status == S_DONE)
        return 2;  // Job already stopped

    Kill(P_PGID(job->pids[0]), SIGTSTP);
    return 0;
}

static int _contjob(int job_id) {
    Job *job = findjob(job_id);
    if (job == NULL)
        return 1;  // Job not found
    if (job->status == S_RUNNING || job->status == S_DONE)
        return 2;  // Job already running

    Kill(P_PGID(job->pids[0]), SIGCONT);
    return 0;
}

static int _termjob(int job_id) {
    Job *job = findjob(job_id);
    if (job == NULL)
        return 1;  // Job not found
    if (job->status == S_DONE)
        return 2;  // Job already terminated

    Kill(P_PGID(job->pids[0]), SIGTERM);
    return 0;
}

static int _deletejobpid(pid_t pid) {
    Job *job = pidfindjob(pid);
    if (job == NULL)
        return 1;  // Job not found

    // Counting the number of terminated processes and marking the terminated process as well
    int nb_term = 0;
    for (int i = 0; i < job->nb_pids; i++) {
        if (job->pids[i] == pid)
            job->pids[i] = P_TERMINATE(job->pids[i]);
        if (P_ISTERMINATED(job->pids[i]))
            nb_term++;
    }

    if (nb_term == job->nb_pids) {     // If all processes of the command have terminated
        if (job->status == S_RUNNING)  // If the job was not already stopped
            job->pausetime = time(NULL);
        job->status = S_DONE;
        if (job == fg) {  // If the job was in foreground, we can free it, otherwise keep it for later notification
            removejob(job->id);
            fg = NULL;
        }
    }
    return 0;
}

static int _contjobpid(pid_t pid) {
    // Must be the pid of the leader process in order to consider it "running" again
    Job *job = pgidfindjob(pid);
    if (job == NULL)
        return 1;  // Job not found

    job->status = S_RUNNING;
    job->starttime += time(NULL) - job->pausetime;
    return 0;
}

static int _stopjobpid(pid_t pid) {
    // Must be the pid of the leader process in order to consider it "stopped"
    Job *job = pgidfindjob(pid);
    if (job == NULL)
        return 1;  // Job not found

    if (job == fg)
        fg = NULL;
    job->status = S_STOPPED;
    job->pausetime = time(NULL);
    return 0;
}

static void _freejobs() {
    JobList *jl = jobs;
    JobList *prev = NULL;
    while (jl != NULL) {
        freejob(jl->job);
        prev = jl;
        jl = jl->next;
        free(prev);
    }
}

static void _killjobs() {
    JobList *jl = jobs;
    while (jl != NULL) {
        if (jl->job->status != S_DONE) {
            Kill(P_PGID(jl->job->pids[0]), SIGKILL);
            for (int i = 0; i < jl->job->nb_pids; i++)
                if (!P_ISTERMINATED(jl->job->pids[i]))
                    Waitpid(jl->job->pids[i], NULL, 0);
        }
        jl = jl->next;
    }
    freejobs();
}

static int _setfg(int job_id) {
    if (fg != NULL)
        return 2;  // A job is already in foreground

    Job *job = findjob(job_id);
    if (job == NULL)
        return 1;  // Job not found

    fg = job;
    return 0;
}

static int _getfg() {
    if (fg == NULL)
        return -1;
    return fg->id;
}

static int _getlastjob() {
    if (jobs == NULL)
        return -1;
    return jobs->job->id;
}

static char *_getjobcmd(int job_id) {
    JobList *jl = jobs;
    while (jl != NULL) {
        if (jl->job->id == job_id)
            return jl->job->cmd;
        jl = jl->next;
    }
    return NULL;
}

static void _printjobs() {
    char *status;
    JobList *jl = jobs;
    while (jl != NULL) {
        switch (jl->job->status) {
            case S_RUNNING:
                status = "Running";
                break;
            case S_STOPPED:
                status = "Suspended";
                break;
            case S_DONE:
                status = "Done";
                break;
            default:
                status = "Unknown";
        }
        printf("[%d] %d  %-9s  %s\n", jl->job->id, P_PID(jl->job->pids[0]), status, jl->job->cmd);
        jl = jl->next;
    }

    // Free the jobs that are "Done", now that they have notified the user of their termination
    jl = jobs;
    JobList *prev = NULL;
    while (jl != NULL) {
        if (jl->job->status != S_DONE) {
            prev = jl;
            jl = jl->next;
            continue;
        }
        if (prev == NULL) {  // First element is "Done"
            jobs = jl->next;
            freejob(jl->job);
            free(jl);
            jl = jobs;
        } else {
            prev->next = jl->next;
            freejob(jl->job);
            free(jl);
            jl = prev->next;
        }
    }
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

int getlastjob() {
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_mask);
    int res = _getlastjob();
    Sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    return res;
}

char *getjobcmd(int job_id) {
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_mask);
    char *res = _getjobcmd(job_id);
    Sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    return res;
}

void printjobs() {
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_mask);
    _printjobs();
    Sigprocmask(SIG_SETMASK, &prev_mask, NULL);
}

void waitfgjob() {
    while (fg != NULL)
        sleep(1);
}
