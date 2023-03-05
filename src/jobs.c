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
    pid_t *pids;       // Array of pids, the pids of the processes executing the commands in the command line, a
                       // negative pid designate a terminated processes
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

static JobList *jobs;                 // Global variable : linked list of jobs
static Job *fg;                       // Global variable : pointer to the foreground job
static sigset_t mask_all, prev_mask;  // Used to block signals until access to global variables is done

/* getnewid - Get a new job identifier
 * Arguments : None
 * Return value : The lowest available job id
 */
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

/* createjoblist - Create a new JobList (element of linked list)
 * Arguments :
 *  - job - A pointer to the Job to add to the JobList
 * Return value : A pointer to the newly created JobList
 */
static JobList *createjoblist(Job *job) {
    JobList *jl = (JobList *) malloc(sizeof(JobList));
    jl->next = NULL;
    jl->job = job;
    return jl;
}

/* createjob - Create a new Job
 * Arguments :
 *  - cmd - The raw command line corresponding to the job
 *  - pids - An array of pids, refer to the struct Job for more information
 *  - nb_pids - The number of pids in the array
 * Return value : A pointer to the newly created Job
 */
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

/* freejob - Free the memory allocated to a Job
 * Arguments :
 *  - job - A pointer to the Job to free
 * Return value : None
 */
static void freejob(Job *job) {
    free(job->cmd);
    free(job->pids);
    free(job);
}

/* removejob - Remove a Job from the linked list of jobs
 * Arguments :
 *  - job_id - The id of the Job to remove
 * Return value : None
 */
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

/* findjob - Find a Job by its id in the linked list of jobs
 * Arguments :
 *  - job_id - The id of the Job to find
 * Return value : A pointer to the Job if found, NULL otherwise
 */
static Job *findjob(int job_id) {
    JobList *jl = jobs;
    while (jl != NULL) {
        if (jl->job->id == job_id)
            return jl->job;
        jl = jl->next;
    }
    return NULL;
}

/* pidfindjob - Find a Job by one of its pid in the linked list of jobs
 * Arguments :
 *  - pid - The pid of one of the processes in the Job to find
 * Return value : A pointer to the Job if found, NULL otherwise
 */
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

/* pgidfindjob - Find a Job by its pgid in the linked list of jobs
 * Arguments :
 *  - pgid - The pgid of the Job to find, that is the pid of the first process of the job
 * Return value : A pointer to the Job if found, NULL otherwise
 */
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

/* _addjob - Add a new Job to the linked list of jobs, not Signal safe version
 * Arguments :
 *  - cmd - The raw command line corresponding to the job
 *  - pids - An array of pids, refer to the struct Job for more information
 *  - nb_pids - The number of pids in the array
 * Return value : The id of the newly created Job
 */
static int _addjob(char *cmd, pid_t *pids, size_t nb_pids) {
    JobList *jl = createjoblist(createjob(cmd, pids, nb_pids));
    jl->next = jobs;
    jobs = jl;
    return jl->job->id;
}

/* _stopjob - Stop a Job (send SIGTSTP to group process), not Signal safe version
 * Arguments :
 *  - job_id - The id of the Job to stop
 * Return value : 0 if the Job was stopped
 *                1 if the Job was not found
 *                2 if the Job was already stopped or terminated
 */
static int _stopjob(int job_id) {
    Job *job = findjob(job_id);
    if (job == NULL)
        return 1;  // Job not found
    else if (job->status == S_STOPPED || job->status == S_DONE)
        return 2;  // Job already stopped

    Kill(P_PGID(job->pids[0]), SIGTSTP);
    return 0;
}

/* _contjob - Continue a Job (send SIGCONT to group process), not Signal safe version
 * Arguments :
 *  - job_id - The id of the Job to continue
 * Return value : 0 if the Job was continued
 *                1 if the Job was not found
 *                2 if the Job was already running or terminated
 */
static int _contjob(int job_id) {
    Job *job = findjob(job_id);
    if (job == NULL)
        return 1;  // Job not found
    if (job->status == S_RUNNING || job->status == S_DONE)
        return 2;  // Job already running

    Kill(P_PGID(job->pids[0]), SIGCONT);
    return 0;
}

/* _termjob - Terminate a Job (send SIGTERM to group process), not Signal safe version
 * Arguments :
 *  - job_id - The id of the Job to terminate
 * Return value : 0 if the Job was terminated
 *                1 if the Job was not found
 *                2 if the Job was already terminated
 */
static int _termjob(int job_id) {
    Job *job = findjob(job_id);
    if (job == NULL)
        return 1;  // Job not found
    if (job->status == S_DONE)
        return 2;  // Job already terminated

    Kill(P_PGID(job->pids[0]), SIGTERM);
    return 0;
}

/* _deletejobpid - Delete a pid from a Job (put it to "Done" state), not Signal safe version
 * Arguments :
 *  - pid - The pid to delete
 * Return value : 0 if the pid was switched to the "terminated" state
 *                1 if the pid was not found
 * Notes : Effectively puts the Job in "Done" state iff all pids of the Job have been treated as "Terminated" beforehand
 */
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

/* _contjobpid - Put back a Job in "Running" state, not Signal safe version
 * Arguments :
 *  - pid - The pid of the Job to continue, only effective if the pid is the pid of the leader process
 * Return value : 0 if the Job was continued
 *                1 if the Job was not found
 */
static int _contjobpid(pid_t pid) {
    // Must be the pid of the leader process in order to consider it "running" again
    Job *job = pgidfindjob(pid);
    if (job == NULL)
        return 1;  // Job not found

    job->status = S_RUNNING;
    job->starttime += time(NULL) - job->pausetime;
    return 0;
}

/* _stopjobpid - Put a Job in "Stopped" state, not Signal safe version
 * Arguments :
 *  - pid - The pid of the Job to stop, only effective if the pid is the pid of the leader process
 * Return value : 0 if the Job was stopped
 *                1 if the Job was not found
 */
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

/* _freejobs - Free all the Jobs, not Signal safe version
 * Arguments : None
 * Return value : None
 */
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

/* _killjobs - Kill all the Jobs (send SIGKILL to all group processes) and free them, not Signal safe version
 * Arguments : None
 * Return value : None
 */
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
    _freejobs();
}

/* _setfg - Set a Job as the foreground Job, not Signal safe version
 * Arguments :
 *  - job_id - The id of the Job to set as the foreground Job
 * Return value : 0 if the Job was set as the foreground Job
 *                1 if the Job was not found
 *                2 if a Job was already in foreground
 */
static int _setfg(int job_id) {
    if (fg != NULL)
        return 2;  // A job is already in foreground

    Job *job = findjob(job_id);
    if (job == NULL)
        return 1;  // Job not found

    fg = job;
    return 0;
}

/* _getfg - Get the id of the foreground Job, not Signal safe version
 * Arguments : None
 * Return value : The id of the foreground Job
 *                -1 if no Job is in foreground
 */
static int _getfg() {
    if (fg == NULL)
        return -1;
    return fg->id;
}

/* _getlastjob - Get the id of the lastly created Job, not Signal safe version
 * Arguments : None
 * Return value : The id of the last Job
 *                -1 if no Job exists
 */
static int _getlastjob() {
    if (jobs == NULL)
        return -1;
    return jobs->job->id;
}

/* _getjob - Get the id of a Job, not Signal safe version
 * Arguments :
 *  - pid - The pid of the Job to get the id
 * Return value : The id of the Job
 *                -1 if the Job was not found
 */
static int _getjob(pid_t pid) {
    Job *job = pidfindjob(pid);
    if (job == NULL)
        return -1;
    return job->id;
}

/* _getjobpgid - Get the pgid of a Job (that is the pid of the process leader), not Signal safe version
 * Arguments :
 *  - job_id - The id of the Job to get the pgid from
 * Return value : The pgid of the Job
 *                -1 if the Job was not found
 */
static pid_t _getjobpgid(int job_id) {
    Job *job = findjob(job_id);
    if (job == NULL)
        return -1;
    return P_PID(job->pids[0]);
}

/* _getjobcmd - Get the command line of a Job, not Signal safe version
 * Arguments :
 *  - job_id - The id of the Job to get the command line from
 * Return value : The raw command line of the Job
 *                NULL if the Job was not found
 */
static char *_getjobcmd(int job_id) {
    JobList *jl = jobs;
    while (jl != NULL) {
        if (jl->job->id == job_id)
            return jl->job->cmd;
        jl = jl->next;
    }
    return NULL;
}

/* _printjobs - Print all the Jobs (nearly as the "jobs" command), not Signal safe version
 *              Also frees the Jobs that are "Done"
 * Arguments : None
 * Return value : None
 */
static void _printjobs() {
    char *status;
    char strtime[9];
    time_t exectime;
    JobList *jl = jobs;
    while (jl != NULL) {
        switch (jl->job->status) {
            case S_RUNNING:
                status = "Running";
                exectime = time(NULL) - jl->job->starttime;
                break;
            case S_STOPPED:
                status = "Suspended";
                exectime = jl->job->pausetime - jl->job->starttime;
                break;
            case S_DONE:
                status = "Done";
                exectime = jl->job->pausetime - jl->job->starttime;
                break;
            default:
                status = "Unknown";
                exectime = 0;
        }
        sprintf(strtime, "%02ld:%02ld:%02ld", exectime / 3600, (exectime % 3600) / 60, exectime % 60); // HH:MM:SS
        printf("[%d] %d  %-9s  %s  %s\n", jl->job->id, P_PID(jl->job->pids[0]), status, strtime, jl->job->cmd);
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


// Public functions : see jobs.h for documentation
// For the most part "Signal safe", that is : Signal handlers are mutually exclusive towards the global variables of
// this file.

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

int getjob(pid_t pid) {
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_mask);
    int res = _getjob(pid);
    Sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    return res;
}

pid_t getjobpgid(int job_id) {
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_mask);
    int res = _getjobpgid(job_id);
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
