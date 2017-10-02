#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>

#define ERR(source) (perror(source),\
		     fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		     exit(EXIT_FAILURE))

void usage(char *name){
	fprintf(stderr,"USAGE: %s fifo_file\n", name);
	exit(EXIT_FAILURE);
}

void read_from_fifo(int fifo){
	ssize_t count;
	char c;
	do{
		if((count=read(fifo,&c,1))<0)ERR("read");
		if(count>0&&isalnum(c)) printf("%c",c);
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
	return EXIT_SUCCESS;
}
/*
Please remember to check for system function (open,close,read etc.) errors or even better all errors. By this you can tell a good programmer from the bad one.
Why there is one second sleep in execution command between a server start and a cat command?
Ad: It allows enough time for the server to start up and create the fifo file "a", without it, the cat command can be first to create "a" as a regular file, then the server would not open a fifo but a regular file instead. You would not notice the problem if "a" already exists and is a FIFO, to reproduce the problem make sure "a" is missing and remove the sleep 1 from the execution commands.
What is the type of "a" in file system and how to check it?
Ad: $ls -l - it is a fifo "p"
Why EEXIST reported by mkfifo is not treated as a critical error?
Ad: We do not remove the fifo file in this stage, if you run the program for the second time "a" will be already in the filesystem and it can be safely reused instead of forcing the user to manually remove it every time.
Isn't reading from fifo char by char inefficient?
Ad: The data is read from the fifo buffer, the only extra overhead includes the one extra function call. It is not the fastest way but it also can not be called very inefficient.
Isn't writing char by char inefficient?
Ad: In this program we write to buffered stream, the extra overhead is minimal, but when you write chat by char to unbuffered descriptor then the overhead becomes a serious problem.
How can you tell that the link does not have and will not have any more data for the reader?
Ad: EOF - broken pipe detected on read occurs when all writing processes/threads disconnect the link and the buffer is depleted.
*/