#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#define ERR(source) (fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
                     perror(source),kill(0,SIGKILL),\
		     		     exit(EXIT_FAILURE))

volatile sig_atomic_t sig_count = 0;

void sethandler( void (*f)(int), int sigNo) {
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = f;
	if (-1==sigaction(sigNo, &act, NULL)) ERR("sigaction");
}

void sig_handler(int sig) {
	sig_count++;;
}

void child_work(int m) {
	struct timespec t = {0, m*10000};
	sethandler(SIG_DFL,SIGUSR1);
	while(1){
		nanosleep(&t,NULL);
		if(kill(getppid(),SIGUSR1))ERR("kill");
	}
}


ssize_t bulk_read(int fd, char *buf, size_t count){
	ssize_t c;
	ssize_t len=0;
	do{
		c=TEMP_FAILURE_RETRY(read(fd,buf,count));
		if(c<0) return c;
		if(c==0) return len; //EOF
		buf+=c;
		len+=c;
		count-=c;
	}while(count>0);
	return len ;
}

ssize_t bulk_write(int fd, char *buf, size_t count){
	ssize_t c;
	ssize_t len=0;
	do{
		c=TEMP_FAILURE_RETRY(write(fd,buf,count));
		if(c<0) return c;
		buf+=c;
		len+=c;
		count-=c;
	}while(count>0);
	return len ;
}

void parent_work(int b, int s, char *name) {
	int i,in,out;
	ssize_t count;
	char *buf=malloc(s);
	if(!buf) ERR("malloc");
	if((out=TEMP_FAILURE_RETRY(open(name,O_WRONLY|O_CREAT|O_TRUNC|O_APPEND,0777)))<0)ERR("open");
	if((in=TEMP_FAILURE_RETRY(open("/dev/urandom",O_RDONLY)))<0)ERR("open");
	for(i=0; i<b;i++){
		if((count=bulk_read(in,buf,s))<0) ERR("read");
		if((count=bulk_write(out,buf,count))<0) ERR("read");
		if(TEMP_FAILURE_RETRY(fprintf(stderr,"Block of %ld bytes transfered. Signals RX:%d\n",count,sig_count))<0)ERR("fprintf");;
	}
	if(TEMP_FAILURE_RETRY(close(in)))ERR("close");
	if(TEMP_FAILURE_RETRY(close(out)))ERR("close");
	free(buf);
	if(kill(0,SIGUSR1))ERR("kill");
}

void usage(char *name){
	fprintf(stderr,"USAGE: %s m b s \n",name);
	fprintf(stderr,"m - number of 1/1000 milliseconds between signals [1,999], i.e. one milisecond maximum\n");
	fprintf(stderr,"b - number of blocks [1,999]\n");
	fprintf(stderr,"s - size of of blocks [1,999] in MB\n");
	fprintf(stderr,"name of the output file\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
	int m,b,s;
	char *name;
	if(argc!=5) usage(argv[0]);
	m = atoi(argv[1]); b = atoi(argv[2]);  s = atoi(argv[3]); name=argv[4];
	if (m<=0||m>999||b<=0||b>999||s<=0||s>999)usage(argv[0]); 
	sethandler(sig_handler,SIGUSR1);
	pid_t pid;
	if((pid=fork())<0) ERR("fork");
	if(0==pid) child_work(m);
	else {
		parent_work(b,s*1024*1024,name);
		while(wait(NULL)>0);
	}
	return EXIT_SUCCESS;
}

/*
Run it with the same parameters as before - flaws are gone now.
For a program to see TEMP_FAILURE_RETRY macro you must first define GNU_SOURCE and then include header file unistd.h .
What error code EINTR represents?
Ad:This is not an error, it is a way for OS to inform the program that the signal handler has been invoked
How should you react to EINTR?
Ad:Unlike real errors do not exit the program, in most cases to recover the problem simply restart the interrupted function with the same set of parameters as in initial call.
At what stage functions are interrupted if EINTR is reported
Ad:Only before they start doing their job - in waiting stage. This means that you can safely restart with the same arguments all the functions used in OPS tutorials except "connect" (OPS2 sockets)
What are other types of interruption signal handler can cause?
Ad:IO transfer can be interrupted in the middle, this case is not reported with EINTR. Sleep and nanosleep similar. In both cases restarting can not reuse the same parameters, it gets complicated.
How do you know what function cat report EINTR?
Ad: Read man pages, error sections. It easy to guess those function must wait before they do their job.
Analyze how bulk_read and bulk_write work. You should know what cases are recognized in those functions, what types of interruption they can handle, how to recognize EOF on the descriptor. It will be discussed during Q&A session but first try on your own, it is a very good exercise.
Both bulk_ functions can be useful not only on signals but also to "glue" IO transfers where data comes from not continuous data sources like pipe/fifo and the socket - it wile be covered by following tutorials.
Not only read/write can be interrupted in the described way, the problem applies to the related pairs like fread/fwrite and send/recv.
As you know SA_RESTART flag can cause automatic restarts on delivery of a signal if this flag is set in the handler, it may not be apparent but this method has a lot of shortcomings:
You must control all the signal handlers used in the code, they all must be set with this flag, if one does not use this flag then you must handle EINTR as usual. It is easy to forget about this requirement if you extend/reuse the older code.
If you try to make some library functions (like bulk_read and write) you can not assume anything about the signals in the caller code.
It is hard to reuse a code depending on SA_RESTART flag, it can only be transferred to the similar strict handler control code.
Sometimes you wish to know about interruption ASAP to react quickly. Sigsuspend would not work if you use this flag!
Why do we not react on other (apart from EINTR) errors of fprintf? If program can not write on stderr (most likely screen) then it cannot report errors.
Really big (f)printfs can get interrupted in the middle of the process (like write). Then it is difficult to restart the process especially if formatting is complicated. Avoid using printf where restarting would be critical (most cases except for the screen output) and the volume of transferred data is significant, use write instead.
*/