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
	printf("[%d] received signal %d\n", getpid(), sig);
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

void child_work(int l) {
	int t,tt;
	srand(getpid());
	t = rand()%6+5; 
	while(l-- > 0){
		for(tt=t;tt>0;tt=sleep(tt));
		if (last_signal == SIGUSR1) printf("Success [%d]\n", getpid());
		else printf("Failed [%d]\n", getpid());
	}
	printf("[%d] Terminates \n",getpid());
}


void parent_work(int k, int p, int l) {
	struct timespec tk = {k, 0};
	struct timespec tp = {p, 0};
	sethandler(sig_handler,SIGALRM);
	alarm(l*10);
	while(last_signal!=SIGALRM) {
		nanosleep(&tk,NULL);
		if (kill(0, SIGUSR1)<0)ERR("kill");
		nanosleep(&tp,NULL);
		if (kill(0, SIGUSR2)<0)ERR("kill");
	}
	printf("[PARENT] Terminates \n");
}

void create_children(int n, int l) {
	while (n-->0) {
		switch (fork()) {
			case 0: sethandler(sig_handler,SIGUSR1);
				sethandler(sig_handler,SIGUSR2);
				child_work(l);
				exit(EXIT_SUCCESS);
			case -1:perror("Fork:");
				exit(EXIT_FAILURE);
		}
	}
}

void usage(void){
	fprintf(stderr,"USAGE: signals n k p l\n");
	fprintf(stderr,"n - number of children\n");
	fprintf(stderr,"k - Interval before SIGUSR1\n");
	fprintf(stderr,"p - Interval before SIGUSR2\n");
	fprintf(stderr,"l - lifetime of child in cycles\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
	int n, k, p, l;
	if(argc!=5) usage();
	n = atoi(argv[1]); k = atoi(argv[2]); p = atoi(argv[3]); l = atoi(argv[4]);
	if (n<=0 || k<=0 || p<=0 || l<=0)  usage(); 
	sethandler(sigchld_handler,SIGCHLD);
	sethandler(SIG_IGN,SIGUSR1);
	sethandler(SIG_IGN,SIGUSR2);
	create_children(n, l);
	parent_work(k, p, l);
	while(wait(NULL)>0);
	return EXIT_SUCCESS;
}

/*
Please notice that sleep and alarm function can conflict, according to POSIX sleep can be implemented on SIGALRM and there is no way to nest signals. Never nest them or use nanosleep instead of sleep as in the code above.
Kill function is invoked with zero pid, means it is sending signal to whole group of processes, we do not need to keep track of pids but do notice that the signal will be delivered to the sender as well!
The location of setting of signal handling and blocking is not trivial, please analyze the example and answer the questions below. Always plan in advance the reactions to signals in your program, this is a common student mistake to overlook the problem.
Why sleep is in a loop, can the sleep time be exact in this case?
Ad:It gets interrupted by signal hadling, restart is a must. Sleep returns the remaining time rounded up to seconds so it can not be precise.
What is default disposition of most of the signals (incl. SIGUSR1 and 2)?
Ad:Most not handled signals will kill the receiver. In this example the lack of handling, ignoring or blocking of SIGUSR1 and 2 would kill the children.
How sending of SIGUSR1 and 2 to the process group affects the program?
Ad:Parent process has to be immune to them, the simplest solution is to ignore them.
What would happen if you turn this ignoring off?
Ad:Parent would kill itself with first signal sent.
Can we shift the signal ignoring setup past the create_children? Child processes set their own signal disposition right at the start, do they need this ignoring?
Ad:They do need it, if you shift the setup and there is no ignoring inherited from the parent process it may happen (rare case but possible) that child process gets created but didn't start its code yet. Immediately after the creation, CPU slice goes to the parent that continues its code and sends the SIGUSR1 signal to the children. If then CPU slice goes back to the child, signal default disposition will kill it before it has a chance to set up its own handler!
Can we modify this program to avoid ignoring in the code?
Ad:In this program both child and a parent can have the same signal handling routines for SIGUSR1 and 2, you can set it just before fork and it will solve the problem.
Would shifting the setup of SIGCHLD handler past the fork change the program? 
Ad:If one of offspring "dies" very quickly (before parent sets its SIGCHLD handler) it will be a zombi until another offspring terminates. It is not a mayor mistake but it's worth attention.
Is wait call at the end of parent really needed? Parent waits long enough for children to finish, right?
Ad:Calculated time may not suffice, in overloaded system expect lags of any duration (few seconds and more), without "wait" children can terminate after the parent because of those lags.
*/