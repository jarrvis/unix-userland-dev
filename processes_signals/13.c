#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#define ERR(source) (fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
                     perror(source),kill(0,SIGKILL),\
		     		     exit(EXIT_FAILURE))


void child_work(int i) {
	srand(time(NULL)*getpid());	
	int t=5+rand()%(10-5+1);
	sleep(t);
	printf("PROCESS with pid %d terminates\n",getpid());
}

void create_children(int n) {
	pid_t s;
	for (n--;n>=0;n--) {
		if((s=fork())<0) ERR("Fork:");
		if(!s) {
			child_work(n);
			exit(EXIT_SUCCESS);
		}
	}
}

void usage(char *name){
	fprintf(stderr,"USAGE: %s 0<n\n",name);
	exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
	int n;
	if(argc<2)  usage(argv[0]);
	n=atoi(argv[1]);
	if(n<=0)  usage(argv[0]);
	create_children(n);
	while(n>0){
		sleep(3);
		pid_t pid;
		for(;;){
			pid=waitpid(0, NULL, WNOHANG);
			if(pid>0) n--;
			if(0==pid) break;
			if(0>=pid) {
				if(ECHILD==errno) break;
				ERR("waitpid:");
			}
		}
		printf("PARENT: %d processes remain\n",n);
	}
	return EXIT_SUCCESS;
}

/*
Use the general makefile (the last one) from the first tutorial, execute "make prog13a"
Make sure you know how the process group is created by shell, what processes belong to it?
Please note that macro ERR was extended with kill(0, SIGKILL), it is meant to terminate the whole program (all other processes) in case of error.
Provide zero as pid argument of kill and you can send a signal to all the processes in the group. It is very useful not to keep the PID's list in your program.
Please notice that we do not test for errors inside of ERR macro (during error reporting), it is so to keep the program action at minimal level at emergency exit. What else can we do ? Call ERR recursively and have the same errors again?
Why after you run this program the command line returns immediately while processes are still working?
Ad.:Parent process is not waiting for child processes, no wait or waitpid call. It will be fixed in the 2nd stage.
How to check the current parent of the created sub-processes (after the initial parent quits)? Why this process?
Ad: Right after the command line returns run: $ps -f, you should see that the PPID (parent PID) is 1 (init/systemd). It is caused by premature end of parent process, the orphaned processes can not "hang" outside of process three so they have to be attached somewhere. To make it simple, it is not the shell but the first process in the system.
Random number generator seed is set in child process, can it be moved to parent process? Will it affect the program?
Ad:Child processes will get the same "random" numbers because they will have the same random seed. Seeding can not be moved to parent.
Can we change the seed from PID to time() call?
Ad:No. Time you get from time() is returned in seconds since 1970, in most cases all sub-processes will have the same seed and will get the same (not random) numbers.
Try to derive a formula to get random number from the range [A,B], it should be obvious.
How this program works if you remove the exit call in child code (right after child_work call)?
Ad:Child process after exiting the child_work will continue back into forking loop! It will start it's own children. Grandchildren can start their children and so on. To mess it up a bit more child processes do not wait for their children.
How many processes will be started in above case if you supply 3 as starting parameter?
Ad: 1 parent 3 children, 3 grand children and 1 grand grand child, 8 in total, draw a process three for it, tag the branches with current (on fork) n value.
What sleep returns? Should we react to this value somehow?
Ad:It returns the time left to sleep at the moment of interruption bu signal handling function. In this code child processes does not receive nor handle the signals so this interruption is not possible. In other codes it may be vital to restart sleep with remaining time.
In the next stage child waiting and child counting will be added. How can we know how many child processes have exited?
Ad:SIGCHLD counting will not be precise as signals can marge, the only sure method is to count successful calls to wait or waitpid.
*/