#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>

#define printerror fprintf(stderr,"ERROR: %s\n",strerror(errno))/*To not print to std_OUT*/
#define READ_END 0
#define WRITE_END 1

int ampersand = 0;
int fd[2];

/*Sorry about the English mistakes in the documentation comments*/

/*Piped fd and checks whether the system-call Succeeded*/
void mypipe() {
	if (pipe(fd) < 0) {/*Return zero on success*/
		perror("Pipe failed :\0");
		printerror;
		exit(0);
	}
}
/*Does execvp and checks whether the system-call Succeeded*/
void myexecvp(char ** arglist) {
	if (execvp(arglist[0], arglist) < 0) {/*Return zero on success*/
		printerror;
		exit(0);
	}
}
/*Wait to pid until finished. checks whether the system-call Succeeded*/
void mywaitpid(pid_t pid) {
	int status;
	if (waitpid(pid, &status, 0) < 0 && errno != ECHILD) {/*Zero on success*/
		printf("ERROR");
		exit(0);
	}
	return;
}
/*Wait function for background processes. Call to mywaitpid from other thread*/
void * mywaitpidthread(void * a) {
	pid_t pid1 = *((int*) a);
	mywaitpid(pid1);
	pthread_exit(NULL);
}
/*The function checks whether the process should run in the background or not.
 *  The function then performs the process that should be run*/
void dowait(pid_t f) {
	if (ampersand) {
		pthread_t thread;
		if (pthread_create(&thread, NULL, mywaitpidthread, &f)) {/*Return zero on success*/
			printf("ERROR");
			exit(0);
		}
	} else {
		struct sigaction sa, old;
		sa.sa_handler = SIG_DFL;
		old.sa_handler = SIG_IGN;
		sigaction(SIGINT, &old, &sa);/*SIG_IGN*/
		mywaitpid(f);
		sigaction(SIGINT, &sa, NULL);/*SIG_DFL*/
	}
}
/* execute the first command in arglist in other processes and wait until the child finished*/
void execute(char** arglist) {
	pid_t childPID;
	mypipe();
	childPID = fork();
	if (childPID < 0) {
		printerror;
	} else if (childPID == 0) {/*Child process*/
		close(fd[READ_END]); /*Duplicate STDOUT_FILENO and close STDIN_FILENO and STDOUT_FILENO*/
		if (dup2(fd[WRITE_END], STDOUT_FILENO) < 0) {
			printerror;
		}
		close(fd[WRITE_END]);
		myexecvp(arglist); /*Execute execvp*/
	} else {/*Parent process*/
		close(fd[WRITE_END]); /*Duplicate STDIN_FILENO and close STDIN_FILENO and STDOUT_FILENO*/
		if (dup2(fd[READ_END], STDIN_FILENO) < 0) {
			printerror;
		}
		close(fd[READ_END]);
		dowait(childPID); /*Wait for the child to finish*/
	}
}
/*Handle incorrectly-placed ampersand (&) and pipe (|) symbols*/
int checkifvalid(int count, char ** argv) {
	int c = 0; //counter
	ampersand = 0; /*restart ampersand*/
	while (argv[c] != NULL) {
		if (!strcmp(argv[c], "&")) {
			ampersand = 1;
			if (c != count - 1 || c == 0)
				return 0;
		}
		if (!strcmp(argv[c], "|")) {
			if (argv[c + 1] == NULL || c == 0 || c == count - 1
					|| !strcmp(argv[c - 1], "|") || !strcmp(argv[c + 1], "&"))
				return 0;
		}
		c++;
	}
	if (c != count) // null in the middle of arglist
		return 0;
	return 1;
}
/*Creates new fork and finds pipes in arglist*/
int process_arglist(int count, char** arglist) {
	int i = 0;
	pid_t f;
	if (!checkifvalid(count, arglist)) { /*Back to main function if there was incorrectly placed ampersand or pipe symbols*/
		printf("Incorrectly placed ampersand or pipe symbols\n");
		return 1;
	}
	if (ampersand) { /*Check whether the operation should be performed in the background and remove the last char(&)*/
		count--;
		arglist[count] = NULL;
	}
	f = fork();
	if (f < 0) {/*Check whatever the fork() action succeeded*/
		printerror;
		exit(0);
	} else if (f == 0) {/*Child process*/
		while (arglist[i] != NULL) {
			if (!strcmp(arglist[i], "|")) {/*Executes the command when finding a pipe*/
				arglist[i] = NULL;
				execute(arglist);
				arglist += 1 + i; /*Promote arglist pointer*/
				i = 0;
			}
			i++;
		}
		myexecvp(arglist); /*Execute the last command*/
	} else {/*Parent process*/
		dowait(f); /*Wait for the child to finish*/
	}
	return 1;
}

// arglist - a list of char* arguments (words) provided by the user
// it contains count+1 items, where the last item (arglist[count]) and *only* the last is NULL
// RETURNS - 1 if should cotinue, 0 otherwise
int process_arglist(int count, char** arglist);

void main(void)
{
	while (1)
	{
		char** arglist = NULL;
		char* line = NULL;
		size_t size;
		int count = 0;

		if (getline(&line, &size, stdin) == -1)
			break;
    
		arglist = (char**) malloc(sizeof(char*));
		if (arglist == NULL) {
			printf("malloc failed: %s\n", strerror(errno));
			exit(-1);
		}
		arglist[0] = strtok(line, " \t\n");
    
		while (arglist[count] != NULL) {
			++count;
			arglist = (char**) realloc(arglist, sizeof(char*) * (count + 1));
			if (arglist == NULL) {
				printf("realloc failed: %s\n", strerror(errno));
				exit(-1);
			}
      
			arglist[count] = strtok(NULL, " \t\n");
		}
    
		if (count != 0) {
			if (!process_arglist(count, arglist)) {
				free(line);
				free(arglist);
				break;
			}
		}
    
		free(line);
		free(arglist);
	}
	pthread_exit(NULL);
}
