#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#define ERR(source) (fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
                     perror(source),kill(0,SIGKILL),\
		     exit(EXIT_FAILURE))

//MAX_BUFF must be in one byte range
#define MAX_BUFF 200

volatile sig_atomic_t last_signal = 0;

int sethandler( void (*f)(int), int sigNo) {
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = f;
	if (-1==sigaction(sigNo, &act, NULL))
		return -1;
	return 0;
}

void sig_handler(int sig) {
	last_signal = sig;
}

void sig_killme(int sig) {
	if(rand()%5==0) 
		exit(EXIT_SUCCESS);
}


void sigchld_handler(int sig) {
	pid_t pid;
	for(;;){
		pid=waitpid(0, NULL, WNOHANG);
		if(0==pid) return;
		if(0>=pid) {
			if(ECHILD==errno) return;
			ERR("waitpid:");
		}
	}
}

void child_work(int fd, int R) {
	char c,buf[MAX_BUFF+1];
	unsigned char s;
	srand(getpid());
	if(sethandler(sig_killme,SIGINT)) ERR("Setting SIGINT handler in child");
	for(;;){
		if(TEMP_FAILURE_RETRY(read(fd,&c,1))<1) ERR("read");
		s=1+rand()%MAX_BUFF;
		buf[0]=s;
		memset(buf+1,c,s);
		if(TEMP_FAILURE_RETRY(write(R,buf,s+1)) <0) ERR("write to R");
	}
}

void parent_work(int n,int *fds,int R) {
	unsigned char c;
	char buf[MAX_BUFF];
	int status,i;
	srand(getpid());
	if(sethandler(sig_handler,SIGINT)) ERR("Setting SIGINT handler in parent");
	for(;;){
		if(SIGINT==last_signal){
			i=rand()%n;
			while(0==fds[i%n]&&i<2*n)i++;
			i%=n;
			if(fds[i]){
				c = 'a'+rand()%('z'-'a');
				status=TEMP_FAILURE_RETRY(write(fds[i],&c,1));
				if(status!=1) {
					if(TEMP_FAILURE_RETRY(close(fds[i]))) ERR("close");
					fds[i]=0;
				}
			}
			last_signal=0;
		}
		status=read(R,&c,1);
		if(status<0&&errno==EINTR) continue;
		if(status<0) ERR("read header from R");
		if(0==status) break;
		if(TEMP_FAILURE_RETRY(read(R,buf,c))<c)ERR("read data from R");
		buf[(int)c]=0;
		printf("\n%s\n",buf);
	}
	
}

void create_children_and_pipes(int n,int *fds,int R) {
	int tmpfd[2];
	int max=n;
	while (n) {
		if(pipe(tmpfd)) ERR("pipe");
		switch (fork()) {
			case 0:
				while(n<max) if(fds[n]&&TEMP_FAILURE_RETRY(close(fds[n++]))) ERR("close");
				free(fds);
				if(TEMP_FAILURE_RETRY(close(tmpfd[1]))) ERR("close");
				child_work(tmpfd[0],R);
				if(TEMP_FAILURE_RETRY(close(tmpfd[0]))) ERR("close");
				if(TEMP_FAILURE_RETRY(close(R))) ERR("close");
				exit(EXIT_SUCCESS);

			case -1: ERR("Fork:");
		}
		if(TEMP_FAILURE_RETRY(close(tmpfd[0]))) ERR("close");
		fds[--n]=tmpfd[1];
	}
}

void usage(char * name){
	fprintf(stderr,"USAGE: %s n\n",name);
	fprintf(stderr,"0<n<=10 - number of children\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
	int n, *fds,R[2];
	if(2!=argc) usage(argv[0]);
	n = atoi(argv[1]);
	if (n<=0||n>10) usage(argv[0]);
	if(sethandler(SIG_IGN,SIGINT)) ERR("Setting SIGINT handler");
	if(sethandler(SIG_IGN,SIGPIPE)) ERR("Setting SIGINT handler");
	if(sethandler(sigchld_handler,SIGCHLD)) ERR("Setting parent SIGCHLD:");
	if(pipe(R)) ERR("pipe");
	if(NULL==(fds=(int*)malloc(sizeof(int)*n))) ERR("malloc");
	create_children_and_pipes(n,fds,R[1]);
	if(TEMP_FAILURE_RETRY(close(R[1]))) ERR("close");
	parent_work(n,fds,R[0]);
	while(n--) if(fds[n]&&TEMP_FAILURE_RETRY(close(fds[n]))) ERR("close");
	if(TEMP_FAILURE_RETRY(close(R[0]))) ERR("close");
	free(fds);
	return EXIT_SUCCESS;
}
/*
Please notice that messages sent via pipe R have variant size - first byte states the message size.
As the size is coded in one byte it cannot exceed 255, the constant MAX_BUFF can not be increased beyond that value. It is not obvious and appropriate comment was added to warn whoever may try to do it. This is an example of absolutely necessary comment in the code.
This task algorithm invalidates the child pipe descriptors as children "die" with 20% probability on each SIGINT. To not try to send the letter to such a "dead" descriptor it must be somehow marked as unused. In this example we use value zero to indicate the inactivity. Zero is a perfectly valid value for descriptor but as it is used for standard input and normally we do not close standard input zero can be used here as we do not expect pipe descriptor to get this value. Close function does not change the descriptor value to zero or -1, we must do it the code.
Random selection of child descriptor must deal with inactive (zero) values. Instead of reselection the program traverses the array in search for the nearby active descriptor. To make the search easier the buffer is wrapped around with modulo operand, to prevent infinite search in case of empty array extra counter cannot exceed the longest distance between the hit and the last element in the array.
Where the parent program waits for the SIGINT signal? There is no blocking, no sigsuspend or sigwait?
Ad: Most of the time program waits for input on the first read in parent's main loop, if it is interrupted by signal it exits with EINTR error.
Please notice that nearly every interruptible function in the code is restarted with TEMP_FAILURE_RETRY macro, all but one above mentioned read, why?
Ad: With macro restarting this read, it would be impossible to react on the delivery of the signal and the program wouldn't work. In this case the interruption is a valid information for us!
Can we use SA_RESTART flag on signal handler to do the automatic restarts and get rid of TEMP_FAILURE_RETRY?
Ad: No, for the same reasons as described in the previous answer. Additionally our code would get less portable as I mentioned in preparation materials for L2 of OPS1.
Why not all C-c keystrokes result in printout from the program?
Ad: It may be due to the chosen child terminating at the same SIGINT (it has 20% chance to do it), signals may merge in the time the main parent loop is still processing the previous C-c and is not waiting on the before mentioned read.
The second reason can be eliminated if you change global variable last_signal to act as the counter that gets increased every time signal arrives, then you can send as many chars to random children as your counter tells you. Please modify the program in this way as an exercise.
Can child process marge the signals?
Ad: Only in theory as the signals are immediately and quickly handled.
Why parent can read the data from the pipe R in parts (first size, then the rest) and child must send data to R in one write? 
Ad: To prevent mixing of the data from various children, see the remarks at the beginning of this tutorial.
Why the program ignores SIGPIPE, is it safe?
Ad: It is not only safe but also necessary, without it the attempt to send the char to "dead" child would kill the whole program. In many cases the fatal termination is NOT the CORRECT WAY of DEALING with BROKEN PIPE!
What is the STOP condition for a main parent loop?
Ad: When parent reads zero bytes from R, it means broken pipe and it can only happen when all children are terminated.
A proper reaction to broken pipe is always important, check if you can indicate all broken pipe checks in the code, both on read and write. How many spots in the code can you find?
Ad: 4
Why we use unsigned char type, what would happen if you remove unsigned from it?
Ad: For buffers larger than 126, buffer sizes read from R would we treated as negative!
Why SIGINT is first ignored in the parent and the proper signal handler function is added after the children are created?
Ad: To prevent the premature end of our program due to quick C-c before it is ready to handle it.
Is SIGCHLD handler absolutely necessary in this code?
Ad: It won't break the logic, but without it zombi will linger and that is something a good programmer would not accept.
*/