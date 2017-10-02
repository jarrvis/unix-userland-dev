#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#define MSG_SIZE (PIPE_BUF - sizeof(pid_t))
#define ERR(source) (perror(source),\
		     fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		     exit(EXIT_FAILURE))


void usage(char *name){
	fprintf(stderr,"USAGE: %s fifo_file file\n", name);
	exit(EXIT_FAILURE);
}

void write_to_fifo(int fifo, int file){
	int64_t  count;
	char buffer[PIPE_BUF];
	char *buf;
	*((pid_t *)buffer)=getpid();
	buf=buffer+sizeof(pid_t);

	do{
		if((count=read(file,buf,MSG_SIZE))<0) ERR("Read:");
		if(count < MSG_SIZE) memset(buf+count,0,MSG_SIZE-count);
		if(count>0) if(write(fifo,buffer,PIPE_BUF)<0) ERR("Write:");
	}while(count==MSG_SIZE);
}

int main(int argc, char** argv) {
	int fifo,file;
	if(argc!=3)  usage(argv[0]);

	if(mkfifo(argv[1], S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP)<0)
		if(errno!=EEXIST) ERR("create fifo");
	if((fifo=open(argv[1],O_WRONLY))<0)ERR("open");
	if((file=open(argv[2],O_RDONLY))<0)ERR("file open");
	write_to_fifo(fifo,file);	
	if(close(file)<0) perror("Close fifo:");
	if(close(fifo)<0) perror("Close fifo:");
	return EXIT_SUCCESS;
}
/*
Please notice how PID is stored in binary format in the buffer. This binary method saves time on conversions from and to text and the result always has the same easy to calculate size (sizeof(pid_t)). Technically it takes only some type casting and cleaver buffer shift to preserve the PID data at the beginning. You can use structures to store more complex variant size data in the same way and provide both communicating programs were compiled in the same way (structures packing problem) it will work.
This time the program opens the fifo for writing instead of reading like server, please remember that fifos are unidirectional.
Why this time sleep was not required in execution commands?
Ad: Now it does not matter who creates the fifo and what is the sequence of it's opening , the other side will always wait. The client program can create the fifo with mkfifo (unlike cat command) and the nonblocking mode is not used.
Why constant size of send messages is important in this program?
Ad: This is the simplest way for the server to know how many bytes comes from one client.
What is memset used for?
Ad: For a quick way to fill the missing part of the last buffer (to the required size of PIPE_BUF) with zeros. Zeros at the end of the string are natural terminators.
Can you send the zeros filling after the last part of the file in separated writes?
Ad: NO, it can mix with the data from the other clients.
How this program will react to broken pipe (fifo in this case but we name any disconnected link in this way) ?
Ad: It will be killed by SIGPIPE.

*/