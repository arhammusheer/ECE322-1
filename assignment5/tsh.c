/*
 * tsh - A tiny shell program with job control
 *
 * Bradley Zylstra 30799320 bzylstra@umass.edu
 * Benjamin Short  30941478 bshort@umass.edu
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
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

struct job_file_table{
	int in;
	int out;
	int err;
	struct job_file_table* next;
};

struct table_list{
	struct job_file_table* head;
	struct job_file_table* tail;
	int size;
};

struct job_t jobs[MAXJOBS]; /* The job list */
struct table_list* table;
int table_list_allocated = 0;
/* End global variables */


/* Function prototypes */

int contains(char* s, char c);
int get_num_jobs(struct job_t* jobs);
int add_new_file_table(struct table_list* table, int sin, int sout, int serr);
struct table_list* init_table_list();
struct job_file_table* table_list_get(struct table_list* table, int index);


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
 * add_new_file_table - adds a new file table to the list. Contains how a command's file table should be redirected
 *
 * Parameters:
 * table - pointer to struct table_list that stores the list
 * sin - file descriptor to replace stdin with. -1 to not replace
 * sout - file descriptor to replace stdout with. -1 to not replace
 * serr - file descriptor to replace stderr with. -1 to not replace
 *
 * Returns:
 * 1 if successfully added to the able. 0 if unsuccessful
*/

int add_new_file_table(struct table_list* table, int sin, int sout, int serr){
	struct job_file_table* ft = (struct job_file_table*) malloc(sizeof(struct job_file_table));
	ft->in = sin;
	ft->out = sout;
	ft->err = serr;
	ft->next = NULL;
	if(table->size == 0){
		table->head = ft;
		table->tail = ft;
		table->size += 1;
	}else{
		table->tail->next = ft;
		table->tail = ft;
		table->size += 1;
	}
	return 0;
}

/*
 * init_table_list - initializes a struct table_list
 *
 * Parameters:
 * table - pointer to struct table_list
 *
*/

struct table_list* init_table_list(){
	struct table_list* t = (struct table_list*) malloc(sizeof(struct table_list));
	table_list_allocated = 1;
	t->head = NULL;
	t->tail = NULL;
	t->size = 0;
	return t;
}

void free_table_list(struct table_list* table){
	if(table_list_allocated){
		struct job_file_table* t = table->head;
		struct job_file_table* t_next;
		while(t != NULL){
			t_next = t->next;
			free(t);
			t = t_next;
		}
		free(table);
		table_list_allocated = 0;
	}
}

struct job_file_table* table_list_get(struct table_list* table, int index){
	if(index > table->size){
		return NULL;
	}
	struct job_file_table* t;
	t = table->head;
	for(int i = 0; i < index; i++){
		t = t->next;
	}
	return t;
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
	char* args[MAXARGS] = {NULL};
	char* commands[10][MAXARGS] = {NULL}; //input handles up to 10 commands
	int numCommands = 1;

	//parse line
	int run_bg = parseline(cmdline, args);
	
	//how to do redirection:
	//parse commandline
	//iterate through the array to find redirection operators
	
	table = init_table_list();
	//increment list index when we find a pipe, because if there is no pipe then there are no more commands we need to worry about,
	//and if there is a pipe then there are more commands to worry about
	int list_index = 0;
	int first_arg_idx = 0;
	int last_arg_idx = 0;
	int found_last_arg = 0; //flags if we have already found the last argument for a command in case of multiple redirectors on the right side
	char in_redir[2] = "<\0";
	char out_redir[2] = ">\0";
	char out_append_redir[3] = ">>\0";
	char pipe_redir[2] = "|\0";
	char err_redir[3] = "2>\0";
	
	int i;
	
	for(i = 0; i < MAXARGS; i++){
		char* arg = args[i];
		if(arg == NULL){
			if(!found_last_arg){
				//reached the end of the arguments list without hitting a final stderr or stdout redirector
				//copy over the rest of the arguments
				last_arg_idx = i - 1; //found index of the last argument
					
				//copy over the arguments
				for(int j = 0; j <= last_arg_idx-first_arg_idx; j++){
					commands[list_index][j] = args[first_arg_idx+j];
				}
				
				//check to see if we have a file table for this job
				struct job_file_table* t= table_list_get(table, list_index);
				if(t == NULL){//our job does not exist yet
					//add our job to the list with the normal file descriptors
					add_new_file_table(table, STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO);
				}
			}
			
			break;
		}
		if(!strncmp(arg, in_redir, 1)){
			if(!found_last_arg){
				last_arg_idx = i - 1; //found index of the last argument
				found_last_arg = 1;
				
				//copy over the arguments
				for(int j = 0; j <= last_arg_idx-first_arg_idx; j++){
					commands[list_index][j] = args[first_arg_idx+j];
				}
			}
			if(i == 0){
				//error
			}else{
				//get previous argument
				char* path = args[i-1];
				
				//obtain file descriptor for the file
				int fd = open(path, O_RDONLY|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO);
				if(fd == -1) printf("%s\n", path);
				
				struct job_file_table* t= table_list_get(table, list_index);
				if(t == NULL){//our job does not exist yet
					//add our job to the list and set its stdin to be from the file
					add_new_file_table(table, fd, STDOUT_FILENO, STDERR_FILENO);
				}else{//our job does exist in the list
					//set job's stdin to be from the file
					t->in = fd;
				}
			}
		}else if (!strncmp(arg, out_redir, 1)){
			if(!found_last_arg){
				last_arg_idx = i - 1; //found index of the last argument
				found_last_arg = 1;
				
				//copy over the arguments
				for(int j = 0; j <= last_arg_idx-first_arg_idx; j++){
					commands[list_index][j] = args[first_arg_idx+j];
				}
			}
			
			if(i == 0){
				//error
			}else{
				//get next argument
				char* path = args[i+1];
				
				//obtain file descriptor for the file
				int fd = open(path, O_WRONLY|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO);
				
				struct job_file_table* t= table_list_get(table, list_index);
				if(t == NULL){//our job does not exist in the list
					//add our job to the list and set its stdout to go to the file
					add_new_file_table(table, STDIN_FILENO, fd, STDERR_FILENO);
				}else{//our job does exist in the list
					//set job's stdout to be to the file
					t->out = fd;
				}
			}
		}else if (!strncmp(arg, pipe_redir, 1)){
			if(!found_last_arg){
				last_arg_idx = i - 1; //found index of the last argument
				found_last_arg = 1;
				
				//copy over the arguments
				for(int j = 0; j <= last_arg_idx-first_arg_idx; j++){
					commands[list_index][j] = args[first_arg_idx+j];
				}
			}
			
			first_arg_idx = i+1; //index of first argument for next command after the pipe
			found_last_arg = 0; //reset found_last_arg for next command
			
			if(i == 0){
				//error
			}else{
				//create pipe
				int fds[2];
				if(!pipe(fds)){
					struct job_file_table* t = table_list_get(table, list_index);
					if(t == NULL){//job does not exist in our list yet
						//add both current and next job to the list
						
						//set out of current job to redirect to the pipe
						add_new_file_table(table, STDIN_FILENO, fds[1], STDERR_FILENO);
						//set in of next job to come from the pipe
						add_new_file_table(table, fds[0], STDOUT_FILENO, STDERR_FILENO);
					}else{//job exists in our list
						//set stdout of the job to redirect to the pipe
						t->out = fds[1];
						if(t->next == NULL){//next job does not exist in our list
							//add new job to the list and set its input to come from the pipe
							add_new_file_table(table, fds[0], STDOUT_FILENO, STDERR_FILENO);
						}else{//next job DOES exist in our list
							//set input of next job to come from the pipe
							t->next->in = fds[0];
						}
					}
					list_index++;
					numCommands++;
				}else{
					//error has occurred
				}
				
			}
		}else if (!strncmp(arg, out_append_redir, 2)){
			if(!found_last_arg){
				last_arg_idx = i - 1; //found index of the last argument
				found_last_arg = 1;
				
				//copy over the arguments
				for(int j = 0; j <= last_arg_idx-first_arg_idx; j++){
					commands[list_index][j] = args[first_arg_idx+j];
				}
			}
			
			
			if(i == 0){
				//error
			}else{
				//get next argument
				char* path = args[i+1];
				
				//obtain file descriptor for the file
				int fd = open(path, O_WRONLY|O_CREAT|O_APPEND, S_IRWXU|S_IRWXG|S_IRWXO);
				
				struct job_file_table* t= table_list_get(table, list_index);
				if(t == NULL){//our job does not exist in the list
					//add our job to the list and set its stdout to go to the file
					add_new_file_table(table, STDIN_FILENO, fd, STDERR_FILENO);
				}else{//our job does exist in the list
					//set job's stdout to be to the file
					t->out = fd;
				}
			}
		}else if (!strncmp(arg, err_redir, 2)){
			if(!found_last_arg){
				last_arg_idx = i - 1; //found index of the last argument
				found_last_arg = 1;
				
				//copy over the arguments
				for(int j = 0; j <= last_arg_idx-first_arg_idx; j++){
					commands[list_index][j] = args[first_arg_idx+j];
				}
			}
			
			if(i == 0){
				//error
			}else{
				//get next argument
				char* path = args[i+1];
				
				//obtain file descriptor for the file
				int fd = open(path, O_WRONLY|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO);
				
				struct job_file_table* t= table_list_get(table, list_index);
				if(t == NULL){//our job does not exist in the list
					//add our job to the list and set its stdout to go to the file
					add_new_file_table(table, STDIN_FILENO, STDOUT_FILENO, fd);
				}else{//our job does exist in the list
					//set job's stdout to be to the file
					t->err = fd;
				}
			}
		}
	}
	
	/*
	printf("Results:\n");
	printf("Number of commands: %d\n", numCommands);
	printf("Commands: \n");
	for(int a = 0; a < numCommands; a++){
		printf("     ");
		for(int b = 0; b < 10; b++){
			if(commands[a][b] == NULL) break;
			printf("%s ", commands[a][b]);
		}
		printf("\n");
	}
	printf("\n");
	*/
	//numCommands is how many commands we have to execute
	//commands array holds the argument array for each command
	//table is a linked list of structs that contain the file descriptors

	//check if built_in command
	if (builtin_cmd(args)) {
		return; //command was built in, we're done here
	}


	pid_t id;
	int job_state = (run_bg) ? BG : FG;

	//figure out full file path vs filename
	int j;
	struct job_file_table* table_struct = table->head;
	for (j = 0; j < numCommands; j++) {
		char* path = malloc(MAXLINE);
		if (contains(commands[j][0], '/')) {//file path
			strcpy(path, commands[j][0]);
		}
		else {//file name
			strcat(path, "/bin/");
			strcat(path, commands[j][0]);
			//strcpy(commands[j][0], path);
		}
		if (get_num_jobs(jobs) < MAXJOBS) {
			id = fork();
			if (id == 0) {
				//child runs job
				setpgid(0, 0);
				// Set stdin;
				dup2(table_struct->in, STDIN_FILENO);
				//close(table_struct->in);

				// set stdout
				//close(1);
				dup2(table_struct->out, STDOUT_FILENO);
				//close(table_struct->out);

				// set stderr
				//close(2);
				dup2(table_struct->err, STDERR_FILENO);
				//close(table_struct->err);
				
				


				if (execve(path, commands[j], environ)) { //I hope environ is the environment for the new program
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
				if (table_struct != table->tail)
					table_struct = table_struct->next;
				
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
	}
	free_table_list(table);
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
	if(killpg(getpgid(fg_id), SIGTSTP)){
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



