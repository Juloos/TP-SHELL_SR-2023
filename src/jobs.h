#ifndef TP_SHELL_SR_2023_JOBS_H
#define TP_SHELL_SR_2023_JOBS_H

#include <sys/types.h>
#include "readcmd.h"

/* initjobs - Initialize the linked list of jobs
 * Arguments : None
 * Return value : None
 */
void initjobs(void);

/* addjob - Add a new Job to the linked list of jobs, Signal safe
 * Arguments :
 *  - cmd - The raw command line corresponding to the job
 *  - pids - An array of pids, refer to the struct Job for more information
 *  - nb_pids - The number of pids in the array
 * Return value : The id of the newly created Job
 */
int addjob(char *cmd, pid_t *pids, size_t nb_pids);

/* stopjob - Stop a Job (send SIGTSTP to group process), Signal safe
 * Arguments :
 *  - job_id - The id of the Job to stop
 * Return value : 0 if the Job was stopped
 *                1 if the Job was not found
 *                2 if the Job was already stopped or terminated
 */
int stopjob(int job_id);

/* contjob - Continue a Job (send SIGCONT to group process), Signal safe
 * Arguments :
 *  - job_id - The id of the Job to continue
 * Return value : 0 if the Job was continued
 *                1 if the Job was not found
 *                2 if the Job was already running or terminated
 */
int contjob(int job_id);

/* termjob - Terminate a Job (send SIGTERM to group process), Signal safe
 * Arguments :
 *  - job_id - The id of the Job to terminate
 * Return value : 0 if the Job was terminated
 *                1 if the Job was not found
 *                2 if the Job was already terminated
 */
int termjob(int job_id);

/* deletejobpid - Delete a pid from a Job (switch it to a "terminated" state), Signal safe
 * Arguments :
 *  - pid - The pid to delete
 * Return value : 0 if the pid was switched to the "terminated" state
 *                1 if the pid was not found
 */
int deletejobpid(pid_t pid);

/* contjobpid - Continue a Job, Signal safe
 * Arguments :
 *  - pid - The pid of the Job to continue, only effective if the pid is the pid of the leader process
 * Return value : 0 if the Job was continued
 *                1 if the Job was not found
 */
int contjobpid(pid_t pid);

/* stopjobpid - Stop a Job, Signal safe
 * Arguments :
 *  - pid - The pid of the Job to stop, only effective if the pid is the pid of the leader process
 * Return value : 0 if the Job was stopped
 *                1 if the Job was not found
 */
int stopjobpid(pid_t pid);

/* freejobs - Free all the Jobs, Signal safe
 * Arguments : None
 * Return value : None
 */
void freejobs(void);

/* killjobs - Kill all the Jobs (send SIGKILL to all group processes) and free them, Signal safe
 * Arguments : None
 * Return value : None
 */
void killjobs(void);

/* setfg - Set a Job as the foreground Job, Signal safe
 * Arguments :
 *  - job_id - The id of the Job to set as the foreground Job
 * Return value : 0 if the Job was set as the foreground Job
 *                1 if the Job was not found
 *                2 if a Job was already in foreground
 */
int setfg(int job_id);

/* getfg - Get the id of the foreground Job, Signal safe
 * Arguments : None
 * Return value : The id of the foreground Job
 *                -1 if no Job is in foreground
 */
int getfg(void);

/* getlastjob - Get the id of the lastly created Job, Signal safe
 * Arguments : None
 * Return value : The id of the last Job
 *                -1 if no Job exists
 */
int getlastjob(void);

/* getjob - Get the id of a Job, Signal safe
 * Arguments :
 *  - pid - The pid of the Job to get the id
 * Return value : The id of the Job
 *                -1 if the Job was not found
 */
int getjob(pid_t pid);

/* getjobpgid - Get the pgid of a Job (that is the pid of the process leader), Signal safe
 * Arguments :
 *  - job_id - The id of the Job to get the pgid from
 * Return value : The pgid of the Job
 *                -1 if the Job was not found
 */
pid_t getjobpgid(int job_id);

/* getjobcmd - Get the command line of a Job, Signal safe
 * Arguments :
 *  - job_id - The id of the Job to get the command line from
 * Return value : The raw command line of the Job
 *                NULL if the Job was not found
 */
char *getjobcmd(int job_id);

/* printjobs - Print all the Jobs (like the "jobs" command), Signal safe
 *              Also frees the Jobs that are "Done"
 * Arguments : None
 * Return value : None
 */
void printjobs(void);

/* waitfgjob - Wait for the foreground Job to finish
 * Arguments : None
 * Return value : None
 */
void waitfgjob(void);

#endif //TP_SHELL_SR_2023_JOBS_H
