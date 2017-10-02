#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#define ERR(source) (fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
                     perror(source),kill(0,SIGKILL),\
		     		     exit(EXIT_FAILURE))

volatile sig_atomic_t last_signal = 0;

void sethandler( void (*f)(int), int sigNo) {
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = f;
	if (-1==sigaction(sigNo, &act, NULL)) ERR("sigaction");
}

void sig_handler(int sig) {
	last_signal = sig;
}

void sigchld_handler(int sig) {
	pid_t pid;
	for(;;){
		pid=waitpid(0, NULL, WNOHANG);
		if(pid==0) return;
		if(pid<=0) {
			if(errno==ECHILD) return;
			ERR("waitpid");
		}
	}
}

void child_work(int m, int p) {
	int count=0;
	struct timespec t = {0, m*10000};
	while(1){
		for(int i =0; i<p; i++){
			nanosleep(&t,NULL);
			if(kill(getppid(),SIGUSR1))ERR("kill");
		}
		nanosleep(&t,NULL);
		if(kill(getppid(),SIGUSR2))ERR("kill");
		count++;
		printf("[%d] sent %d SIGUSR2\n",getpid(), count);

	}
}


void parent_work(sigset_t oldmask) {
	int count=0;
	while(1){
		last_signal=0;
		while(last_signal!=SIGUSR2)
			sigsuspend(&oldmask);
		count++;
		printf("[PARENT] received %d SIGUSR2\n", count);
		
	}
}

void usage(char *name){
	fprintf(stderr,"USAGE: %s m  p\n",name);
	fprintf(stderr,"m - number of 1/1000 milliseconds between signals [1,999], i.e. one milisecond maximum\n");
	fprintf(stderr,"p - after p SIGUSR1 send one SIGUSER2  [1,999]\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
	int m,p;
	if(argc!=3) usage(argv[0]);
	m = atoi(argv[1]); p = atoi(argv[2]);
	if (m<=0 || m>999 || p<=0 || p>999)  usage(argv[0]); 
	sethandler(sigchld_handler,SIGCHLD);
	sethandler(sig_handler,SIGUSR1);
	sethandler(sig_handler,SIGUSR2);
	sigset_t mask, oldmask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGUSR1);
	sigaddset(&mask, SIGUSR2);
	sigprocmask(SIG_BLOCK, &mask, &oldmask);
	pid_t pid;
	if((pid=fork())<0) ERR("fork");
	if(0==pid) child_work(m,p);
	else {
		parent_work(oldmask);
		while(wait(NULL)>0);
	}
	sigprocmask(SIG_UNBLOCK, &mask, NULL);
	return EXIT_SUCCESS;
};


/*
Do remember that you can read good quality really random bytes from /dev/random file but the amount is limited or read unlimited amount of data from /dev/urandom but these are pseudo random bytes.
You should see the following flaws if you run the program with 1 20 40 out.txt params:
Coping of blocks shorter than 40Mb, in my case it was at most 33554431, it is due to signal interruption DURING the IO operation
fprintf: Interrupted system call - function interrupted by signal handling BEFORE it did anything
Similar messages for open and close - it may be hard to observe in this program but it is possible and described by POSIX documentation.
How to get rid of those flows is explained in the 2nd stage.
If there is memory allocation in your code there MUST also be memory release! Always.
Permissions passed to open function can also be expressed with predefined constants (man 3p mknod). As octal permission representation is well recognized by programmers and administrators it can also be noted in this way and will not be considered as "magic number" style mistake. It is fairly easy to trace those constants in the code.
Obviously the parent counts less signals than child sends, as summing runs inside the handler we can only blame merging for it. Can you tell why signal merging is so strong in this code?
Ad:In this architecture (GNU/Linux) CPU planer blocks signals during IO operations (to some size as we can see) and during IO signals have more time to merge.
What for the SIGUSR1 is sent to the process group at the end of the parent process?
Ad:To terminate the child.
How come it works? SIGUSR1 handling is inherited from the parent?
Odp:Child first action is to restore default signal disposition - killing of the receiver.
Why parent does not kill itself with this signal?
Ad:It sets the handler for SIGUSR1 before it sends it to the group.
Can this strategy fail?
Ad:Yes, if parent process finishes it's job before child is able to even start the code and reset SIGUSR1 disposition.
Can you improve it and at the same time not kill the parent with the signal from a child?
Ad:send SIGUSR2 to the child.
Is this child (children) termination strategy easy and correct at the same time in all possible programs?
Ad:Only if child processe does not have resources to release, if it has something to release you must add proper signal handling and this may be complicated.
Why to check if a pointer to newly allocated memory is not null?
Ad:Operating system may not be able to grant your program additional memory, in this case it reports the error with the NULL. You must be prepared for it. The lack of this check is a common students' mistake.
Can you turn the allocated buffer into automatic variable and avoid the effort of allocating and releasing the memory?
Ad:I don't know about OS architecture that uses stacks large enough to accommodate 40MB, typical stack has a few MB at most. For smaller buffers (a few KB) it can work.
Why permissions of a newly created file are supposed to be full (0777)? Are they really full?
Ad:umask will reduce the permissions, if no set permissions are required it is a good idea to allow the umask to regulate the effective rights
*/