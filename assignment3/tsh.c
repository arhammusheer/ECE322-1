/*
 * tsh - A tiny shell program with job control
 *
 * Bradley Zylstra 30799320 bzylstra@umass.edu
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

 /* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */


/*
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

 /* Global variables */
extern char** environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */


struct job_t {              /* The job struct */
	pid_t pid;              /* job PID */
	int jid;                /* job ID [1, 2, ...] */
	int state;              /* UNDEF, BG, FG, or ST */
	char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */


/* Function prototypes */

int contains(char* s, char c);
int get_num_jobs(struct job_t* jobs);

/* Here are the functions that you will implement */
void eval(char* cmdline);
int builtin_cmd(char** argv);
void do_bgfg(char** argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char* cmdline, char** argv);
void sigquit_handler(int sig);

void clearjob(struct job_t* job);
void initjobs(struct job_t* jobs);
int maxjid(struct job_t* jobs);
int addjob(struct job_t* jobs, pid_t pid, int state, char* cmdline);
int deletejob(struct job_t* jobs, pid_t pid);
pid_t fgpid(struct job_t* jobs);
struct job_t* getjobpid(struct job_t* jobs, pid_t pid);
struct job_t* getjobjid(struct job_t* jobs, int jid);
int pid2jid(pid_t pid);
void listjobs(struct job_t* jobs);
void listjob(struct job_t* jobs);
void usage(void);
void unix_error(char* msg);
void app_error(char* msg);
typedef void handler_t(int);
handler_t* Signal(int signum, handler_t* handler);

/*
 * main - The shell's main routine
 */
int main(int argc, char** argv)
{
	char c;
	char cmdline[MAXLINE];
	int emit_prompt = 1; /* emit prompt (default) */

	/* Redirect stderr to stdout (so that driver will get all output
	 * on the pipe connected to stdout) */
	dup2(1, 2);

	/* Parse the command line */
	while ((c = getopt(argc, argv, "hvp")) != EOF) {
		switch (c) {
		case 'h':             /* print help message */
			usage();
			break;
		case 'v':             /* emit additional diagnostic info */
			verbose = 1;
			break;
		case 'p':             /* don't print a prompt */
			emit_prompt = 0;  /* handy for automatic testing */
			break;
		default:
			usage();
		}
	}

	/* Install the signal handlers */

	/* These are the ones you will need to implement */
	Signal(SIGINT, sigint_handler);   /* ctrl-c */
	Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
	Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

	/* This one provides a clean way to kill the shell */
	Signal(SIGQUIT, sigquit_handler);

	/* Initialize the job list */
	initjobs(jobs);

	/* Execute the shell's read/eval loop */
	while (1) {

		/* Read command line */
		if (emit_prompt) {
			printf("%s", prompt);
			fflush(stdout);
		}
		if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
			app_error("fgets error");
		if (feof(stdin)) { /* End of file (ctrl-d) */
			fflush(stdout);
			exit(0);
		}

		/* Evaluate the command line */
		eval(cmdline);
		fflush(stdout);
		fflush(stdout);
	}

	exit(0); /* control never reaches here */
}

/*
 * contains - Returns whether a string contains a given character
 *
 * Parameters:
 * s - string
 * c - character to search for
 *
 * Returns:
 * 1 if string contains the character
 * 0 if character not found in the string
*/

int contains(char* s, char c) {
	int idx = 1;
	char iter = s[0];
	while (iter != '\0') {
		if (iter == c) {
			return 1;
		}
		iter = s[idx++];
	}
	return 0;
}

/*
 * get_num_jobs - Iterates through a job array to count the number of scheduled jobs
 *
 * Parameters:
 * jobs - pointer to job struct array
 *
 * Returns:
 * number of jobs whose state is not UNDEF
*/

int get_num_jobs(struct job_t* jobs) {
	int count = 0;
	for (int i = 0; i < MAXJOBS; i++) {
		if (jobs[i].state != UNDEF) {
			count++;
		}
	}
	return count;
}


/*
 * eval - Evaluate the command line that the user has just typed in
 *
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.
*/
void eval(char* cmdline) //Ben
{
	char* args[MAXARGS]; //hopefully not more than 50 arguments on the command line



	//parse line
	int run_bg = parseline(cmdline, args);

	//check if built_in command
	if (builtin_cmd(args)) {
		return; //command was built in, we're done here
	}

	pid_t id;
	int job_state = (run_bg) ? BG : FG;

	//figure out full file path vs filename
	char *path = malloc(MAXLINE);
	if (contains(args[0], '/')) {//file path
		strcpy(path, args[0]);
	}
	else {//file name
	    strcat(path, "/bin/");
		strcat(path, args[0]);
	}
	if (get_num_jobs(jobs) < MAXJOBS) {
		id = fork();
		if (id == 0) {
			//child runs job
			setpgid(0,0);
			if(execve(path, args, environ)){ //I hope environ is the environment for the new program
				//if returns, then there was some error
				char buffer[50];
				sprintf(buffer, "%s: Command not found", path);
				puts(buffer);
				exit(0);
			}
		}
		else {//parent
		   //add job to list
            addjob(jobs, id, job_state, cmdline);
            struct job_t* requested_job = getjobpid(jobs, id);
			if (!run_bg) {
				//parent needs to display stdout of child? how to do?

				//wait for foreground process to complete
				waitfg(id);
			}
			else {
				//program in background, parent continues as normal
				listjob(requested_job);
			}
			
		}
	}
	else {
		//job list already full!

	}
	memset(path, 0, strlen(path));
    free(path);
	return;
}

/*
 * parseline - Parse the command line and build the argv array.
 *
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.
 */
int parseline(const char* cmdline, char** argv)
{
	static char array[MAXLINE]; /* holds local copy of command line */
	char* buf = array;          /* ptr that traverses command line */
	char* delim;                /* points to first space delimiter */
	int argc;                   /* number of args */
	int bg;                     /* background job? */

	strcpy(buf, cmdline);
	buf[strlen(buf) - 1] = ' ';  /* replace trailing '\n' with space */
	while (*buf && (*buf == ' ')) /* ignore leading spaces */
		buf++;

	/* Build the argv list */
	argc = 0;
	if (*buf == '\'') {
		buf++;
		delim = strchr(buf, '\'');
	}
	else {
		delim = strchr(buf, ' ');
	}

	while (delim) {
		argv[argc++] = buf;
		*delim = '\0';
		buf = delim + 1;
		while (*buf && (*buf == ' ')) /* ignore spaces */
			buf++;

		if (*buf == '\'') {
			buf++;
			delim = strchr(buf, '\'');
		}
		else {
			delim = strchr(buf, ' ');
		}
	}
	argv[argc] = NULL;

	if (argc == 0)  /* ignore blank line */
		return 1;

	/* should the job run in the background? */
	if ((bg = (*argv[argc - 1] == '&')) != 0) {
		argv[--argc] = NULL;
	}
	return bg;
}

/*
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.
 */
int builtin_cmd(char** argv)
{
	char builtin_cmds[4][5] = { "fg", "bg", "quit", "jobs" };
	int job_type = 0;
	// Loop through the arg commands and check for builtin commands
    for (int j = 0; j < 4; j++) {
        if (strcmp(*argv, builtin_cmds[j]) == 0)
            job_type = j + 1;
    }

	if (job_type == 0) {
		return 0; /*No builtin command detected*/
	}

	if (job_type < 3) {
		do_bgfg(argv); // Jobtype less than 3 means either fg or bg job
		return 1;
	}
	if (job_type == 4) {
		listjobs(jobs); // List jobs command
		return 1;
	}
	if (job_type == 3) {
		exit(0); // Quit command so exit the shell
	}
	return 1;     /*Program never reaches here*/
}

/*
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char** argv)
{
    static char arg2[MAXLINE];
	int is_job_id = 0;
    struct job_t* requested_job;
    pid_t job_val;
    // If there is no second argument, print error and quit
    if(!*(argv + 1)){
		char buffer[50];
		sprintf(buffer, "%s command requires PID or %%jobid argument", argv[0]);
        puts(buffer);
       return;
    }
	strcpy(arg2, *(argv + 1));
    // Get the integer value from the second argument, could be either PID or JID
	if(arg2[0] == '%'){
		is_job_id = 1;
		job_val = atoi(strtok(arg2, "%"));
	}else{
		job_val = atoi(arg2);
	}
	// If the integer is invalid, quit
	if(!job_val){
		char buffer[50];
		sprintf(buffer, "%s: argument must be a PID or %%jobid", argv[0]);
        puts(buffer);
       return;
	}
    strcpy(arg2, *(argv + 1));
	// If in job ID form, do the processing
    if(is_job_id){
        requested_job = getjobjid(jobs, job_val);
		if(requested_job == NULL){
			// Job not found
            char buffer[50];
			sprintf(buffer, "%%%i: No such job", job_val);
			puts(buffer);
			return;
		}
    }

    else{
        // job param is in pid form
        job_val = (pid_t) atoi(arg2);
        requested_job = getjobpid(jobs, job_val);
		if(requested_job == NULL){
			// Job not found
            char buffer[50];
			sprintf(buffer, "(%i): No such process", job_val);
			puts(buffer);
			return;
		}
    }


    // Get the pid of the requested job in case it was previously in JID form
	job_val = requested_job->pid;
    if(strcmp("bg", argv[0]) == 0){
        // Run background case
        requested_job->state = BG;
        if(killpg(getpgid(job_val), SIGCONT)){
            puts("killpg in do_bgfg failed to run!");
            return;
        }

		listjob(requested_job);
    }
    if(strcmp("fg", argv[0])==0){
        // Run Foreground case
        requested_job->state = FG;
        if(killpg(getpgid(job_val), SIGCONT)){
            puts("killpg in do_bgfg failed to run!");
            return;
        }
        waitfg(job_val);
    }
	return;
}

/*
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid) //Ben
{
	sigset_t prev_mask;
	sigemptyset(&prev_mask);
	while(fgpid(jobs) == pid)
	    sigsuspend(&prev_mask);

	return;
}

/*****************
 * Signal handlers
 *****************/

 /*
  * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
  *     a child job terminates (becomes a zombie), or stops because it
  *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
  *     available zombie children, but doesn't wait for any other
  *     currently running children to terminate.
  */
void sigchld_handler(int sig)
{
    // Save old error and create locals for later use
	int olderrno = errno; // Save old error
	pid_t pid;
	int process_status;

	// Create 2 masks and fill mask to block
	sigset_t mask, prev_mask;
	sigfillset(&mask);
	// Request status for all zombie child processes, dont block the program execution, and report stopped processes
	// If valid PID is found, run below code
	if((pid = waitpid(-1, &process_status, WNOHANG | WUNTRACED)) > 0) {
	    // Block any new signals during processing
        if(sigprocmask(SIG_BLOCK, &mask, &prev_mask)){
            puts("sigprocmask in sigchld_handler failed to run!");
            return;
        }
        // Get job_t struct and check if job has been removed, if so then return from this function
        struct job_t *fg_job = getjobpid(jobs, pid);
        if(!fg_job){
            // Set mask to old mask
            if(sigprocmask(SIG_SETMASK, &prev_mask, NULL)){
                puts("sigprocmask in sigchld_handler failed to run!");
            };
            return;
        }
        // Store PID and JID
        int j_id = fg_job->jid;
        int j_pid = fg_job->pid;

        if (WIFEXITED(process_status)) {
            // Normal Termination in this block, just remove job from joblist
            deletejob(jobs, pid);
        }
        else if (WIFSIGNALED(process_status)) {
            // Uncaught signal exit, most likely SIGINT. Remove job from joblist
            char buffer[50];
            sprintf(buffer, "Job [%i] (%i) terminated by signal 2", j_id, j_pid);
            puts(buffer);
            deletejob(jobs, pid);
        }
        else if (WIFSTOPPED(process_status)) {
            // Stopped process. Set jobstate to Stopped unless jobstate is already stopped
            if(fg_job->state == ST){
                if(sigprocmask(SIG_SETMASK, &prev_mask, NULL)){
                    puts("sigprocmask in sigchld_handler failed to run!");
                }
                return;
            }
            fg_job->state = ST;
            char buffer[50];
            sprintf(buffer, "Job [%i] (%i) stopped by signal 20", j_id, j_pid);
            puts(buffer);
        }
        // Set the signal mask back to the old value
        if(sigprocmask(SIG_SETMASK, &prev_mask, NULL)){
            puts("sigprocmask in sigchld_handler failed to run!");
            return;
        }
    }

	errno = olderrno; // Return error to prior value
	return;
}

/*
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.
 */
void sigint_handler(int sig) //Ben
{
	pid_t fg_id = fgpid(jobs);
	struct job_t* fg_job = getjobpid(jobs, fg_id);
	int j_id = fg_job->jid;
	int j_pid = fg_job->pid;
	char buffer[50];
	sprintf(buffer, "Job [%i] (%i) terminated by signal %i", j_id, j_pid, sig);
	puts(buffer);
    sigset_t mask, prev_mask;
    sigfillset(&mask);
    if(sigprocmask(SIG_BLOCK, &mask, &prev_mask)){
		//if error in sigprocmask
		int err = errno;
		char buffer[50];
		sprintf(buffer, "Error in sigprocmask: %i", err);
		puts(buffer);
	}
	if(killpg(getpgid(fg_id), SIGINT)){
		//if error in killpg
		int err = errno;
		char buffer[50];
		sprintf(buffer, "Error in killpg: %i", err);
		if(getpgid(fg_id)){
			//error was actually due to getpgid
			int err2 = errno;
			sprintf(buffer, "Error in getpgid: %i", err2);
		}
		puts(buffer);
	}
    if(sigprocmask(SIG_SETMASK, &prev_mask, NULL)){
		//if error in sigprocmask
		int err = errno;
		char buffer[50];
		sprintf(buffer, "Error in sigprocmask: %i", err);
		puts(buffer);
	}
	deletejob(jobs, fg_id);
	return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.
 */
void sigtstp_handler(int sig) //Ben
{
	pid_t fg_id = fgpid(jobs);
	struct job_t* fg_job = getjobpid(jobs, fg_id);
	int j_id = fg_job->jid;
	int j_pid = fg_job->pid;
	char buffer[50];
	sprintf(buffer, "Job [%i] (%i) stopped by signal %i", j_id, j_pid, sig);
	puts(buffer);
	fg_job->state = ST;
	//fg_job.pid = 0;
    sigset_t mask, prev_mask;
    sigfillset(&mask);
    if(sigprocmask(SIG_BLOCK, &mask, &prev_mask)){
		//if error in sigprocmask
		int err = errno;
		char buffer[50];
		sprintf(buffer, "Error in sigprocmask: %i", err);
		puts(buffer);
	}
	if(killpg(getpgid(fg_id), SIGINT)){
		//if error in killpg
		int err = errno;
		char buffer[50];
		sprintf(buffer, "Error in killpg: %i", err);
		if(getpgid(fg_id)){
			//error was actually due to getpgid
			int err2 = errno;
			sprintf(buffer, "Error in getpgid: %i", err2);
		}
		puts(buffer);
	}
    if(sigprocmask(SIG_SETMASK, &prev_mask, NULL)){
		//if error in sigprocmask
		int err = errno;
		char buffer[50];
		sprintf(buffer, "Error in sigprocmask: %i", err);
		puts(buffer);
	}
	return;
}

/*********************
 * End signal handlers
 *********************/

 /***********************************************
  * Helper routines that manipulate the job list
  **********************************************/

  /* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t* job) {
	job->pid = 0;
	job->jid = 0;
	job->state = UNDEF;
	job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t* jobs) {
	int i;

	for (i = 0; i < MAXJOBS; i++)
		clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t* jobs)
{
	int i, max = 0;

	for (i = 0; i < MAXJOBS; i++)
		if (jobs[i].jid > max)
			max = jobs[i].jid;
	return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t* jobs, pid_t pid, int state, char* cmdline)
{
	int i;

	if (pid < 1)
		return 0;

	for (i = 0; i < MAXJOBS; i++) {
		if (jobs[i].pid == 0) {
			jobs[i].pid = pid;
			jobs[i].state = state;
			jobs[i].jid = nextjid++;
			if (nextjid > MAXJOBS)
				nextjid = 1;
			strcpy(jobs[i].cmdline, cmdline);
			if (verbose) {
				printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
			}
			return 1;
		}
	}
	printf("Tried to create too many jobs\n");
	return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t* jobs, pid_t pid)
{
	int i;

	if (pid < 1)
		return 0;

	for (i = 0; i < MAXJOBS; i++) {
		if (jobs[i].pid == pid) {
			clearjob(&jobs[i]);
			nextjid = maxjid(jobs) + 1;
			return 1;
		}
	}
	return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t* jobs) {
	int i;

	for (i = 0; i < MAXJOBS; i++)
		if (jobs[i].state == FG)
			return jobs[i].pid;
	return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t* getjobpid(struct job_t* jobs, pid_t pid) {
	int i;

	if (pid < 1)
		return NULL;
	for (i = 0; i < MAXJOBS; i++)
		if (jobs[i].pid == pid)
			return &jobs[i];
	return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t* getjobjid(struct job_t* jobs, int jid)
{
	int i;

	if (jid < 1)
		return NULL;
	for (i = 0; i < MAXJOBS; i++)
		if (jobs[i].jid == jid)
			return &jobs[i];
	return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid)
{
	int i;

	if (pid < 1)
		return 0;
	for (i = 0; i < MAXJOBS; i++)
		if (jobs[i].pid == pid) {
			return jobs[i].jid;
		}
	return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t* jobs)
{
	int i;

	for (i = 0; i < MAXJOBS; i++) {
		if (jobs[i].pid != 0) {
			printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
			switch (jobs[i].state) {
			case BG:
				printf("Running ");
				break;
			case FG:
				printf("Foreground ");
				break;
			case ST:
				printf("Stopped ");
				break;
			default:
				printf("listjobs: Internal error: job[%d].state=%d ",
					i, jobs[i].state);
			}
			printf("%s", jobs[i].cmdline);
		}
	}
}
void listjob(struct job_t* jobs)
{
    int i;

        if (jobs[0].pid != 0) {
            printf("[%d] (%d) ", jobs[0].jid, jobs[0].pid);
            printf("%s", jobs[0].cmdline);
        }

}
/******************************
 * end job list helper routines
 ******************************/


 /***********************
  * Other helper routines
  ***********************/

  /*
   * usage - print a help message
   */
void usage(void)
{
	printf("Usage: shell [-hvp]\n");
	printf("   -h   print this message\n");
	printf("   -v   print additional diagnostic information\n");
	printf("   -p   do not emit a command prompt\n");
	exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char* msg)
{
	fprintf(stdout, "%s: %s\n", msg, strerror(errno));
	exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char* msg)
{
	fprintf(stdout, "%s\n", msg);
	exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t* Signal(int signum, handler_t* handler)
{
	struct sigaction action, old_action;

	action.sa_handler = handler;
	sigemptyset(&action.sa_mask); /* block sigs of type being handled */
	action.sa_flags = SA_RESTART; /* restart syscalls if possible */

	if (sigaction(signum, &action, &old_action) < 0)
		unix_error("Signal error");
	return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig)
{
    puts("Terminating after receipt of SIGQUIT signal\n");
	exit(1);
}



