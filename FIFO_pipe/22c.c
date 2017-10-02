#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <limits.h>

#define ERR(source) (perror(source),\
		     fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		     exit(EXIT_FAILURE))

void usage(char *name){
	fprintf(stderr,"USAGE: %s fifo_file\n", name);
	exit(EXIT_FAILURE);
}

void read_from_fifo(int fifo){
	ssize_t count,i;
	char buffer[PIPE_BUF];
	do{
		if((count=read(fifo,buffer,PIPE_BUF))<0)ERR("read");
		if(count>0){
			printf("\nPID:%d-------------------------------------\n",*((pid_t*)buffer));
			for(i=sizeof(pid_t);i<PIPE_BUF;i++)
				if(isalnum(buffer[i])) printf("%c",buffer[i]);
		}
	}while(count>0);
}

int main(int argc, char** argv) {
	int fifo;
	if(argc!=2) usage(argv[0]);

	if(mkfifo(argv[1], S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP)<0)
		if(errno!=EEXIST) ERR("create fifo");
	if((fifo=open(argv[1],O_RDONLY))<0)ERR("open");
	read_from_fifo(fifo);	
	if(close(fifo)<0) ERR("close fifo:");
	if(unlink(argv[1])<0)ERR("remove fifo:");
	return EXIT_SUCCESS;
}
/*
Please notice that broken pipe in parent does not need to terminate the program, it must stop using this one pipe instead.
200 bytes is the maximum buffer size, will it be atomic? On Linux and all POSIX compatible platforms yes but the problem is more complex, what if you need a bit larger buffers? You can derive your size from PIPE_BUF (e.g. PIPE_BUFF/4) or test your max size against the PIPE_BUF and if it exceeds make it equal to PIPE_BUF.
It is important to close all unused descriptors, this pipe program has a lot to close, please make sure you know which descriptors are to be closed right at the beginning.
Likewise descriptors unused memory should be released, make sure you know what heap parts should be deallocated in "child" process.
[a-z] characters randomization should be obvious. If it is not try to build more general formula from this example on paper, try to apply it to other ranges of chars and numbers.
Sometimes (set n=10 for most frequent observation) program stops with the message : "Interrupted system call" , why? 
Ad: SIGCHLD handling interrupts read before it can read anything.
How can we protect the code from this interruption? 
Ad: The simplest solution would be to add macro: TEMP_FAILURE_RETRY(read(...)).
How the program reacts on broken pipe R?
Ad: This is natural end of the main loop and the program because it happens when all the children disconnect from R, they do it when they are terminating.
Why the parent process do not wait for children at the end of the process code? 
Ad: In this code all the children must terminate to end the main parent loop, when the parent reaches the end of code there are no children to wait for as they all must have been waited for by SIGCHILD handler.
*/