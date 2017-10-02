#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <aio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#define BLOCKS 3
#define SHIFT(counter, x) ((counter + x) % BLOCKS)
void error(char *);
void usage(char *);
void siginthandler(int);
void sethandler(void (*)(int), int);
off_t getfilelength(int);
void fillaiostructs(struct aiocb *, char **, int, int);
void suspend(struct aiocb *);
void readdata(struct aiocb *, off_t);
void writedata(struct aiocb *, off_t);
void syncdata(struct aiocb *);
void getindexes(int *, int);
void cleanup(char **, int);
void reversebuffer(char *, int);
void processblocks(struct aiocb *, char **, int, int, int);
volatile sig_atomic_t work;
void error(char *msg){
	perror(msg);
	exit(EXIT_FAILURE);
}
void usage(char *progname){
	fprintf(stderr, "%s workfile blocksize\n", progname);
	fprintf(stderr, "workfile - path to the file to work on\n");
	fprintf(stderr, "n - number of blocks\n");
	fprintf(stderr, "k - number of iterations\n");
	exit(EXIT_FAILURE);
}
void siginthandler(int sig){
	work = 0;
}
void sethandler(void (*f)(int), int sig){
	struct sigaction sa;
	memset(&sa, 0x00, sizeof(struct sigaction));
	sa.sa_handler = f;
	if (sigaction(sig, &sa, NULL) == -1)
		error("Error setting signal handler");
}
off_t getfilelength(int fd){
	struct stat buf;
	if (fstat(fd, &buf) == -1)
		error("Cannot fstat file");
	return buf.st_size;
}
void suspend(struct aiocb *aiocbs){
	struct aiocb *aiolist[1];
	aiolist[0] = aiocbs;
	if (!work) return;
	while (aio_suspend((const struct aiocb *const *) aiolist, 1, NULL) == -1){
		if (!work) return;
		if (errno == EINTR) continue;
		error("Suspend error");
	}
	if (aio_error(aiocbs) != 0)
		error("Suspend error");
	if (aio_return(aiocbs) == -1)
		error("Return error");
}
void fillaiostructs(struct aiocb *aiocbs, char **buffer, int fd, int blocksize){
	int i;
	for (i = 0; i<BLOCKS; i++){
		memset(&aiocbs[i], 0, sizeof(struct aiocb));
		aiocbs[i].aio_fildes = fd;
		aiocbs[i].aio_offset = 0;
		aiocbs[i].aio_nbytes = blocksize;
		aiocbs[i].aio_buf = (void *) buffer[i];
		aiocbs[i].aio_sigevent.sigev_notify = SIGEV_NONE;
	}}
void readdata(struct aiocb *aiocbs, off_t offset){
	if (!work) return;
	aiocbs->aio_offset = offset;
	if (aio_read(aiocbs) == -1)
		error("Cannot read");
}
void writedata(struct aiocb *aiocbs, off_t offset){
	if (!work) return;
	aiocbs->aio_offset = offset;
	if (aio_write(aiocbs) == -1)
		error("Cannot write");
}
void syncdata(struct aiocb *aiocbs){
	if (!work) return;
	suspend(aiocbs);
	if (aio_fsync(O_SYNC, aiocbs) == -1)
		error("Cannot sync\n");
	suspend(aiocbs);
}
void getindexes(int *indexes, int max){
	indexes[0] = rand() % max;
	indexes[1] = rand() % (max - 1);
	if (indexes[1] >= indexes[0])
		indexes[1]++;
}
void cleanup(char **buffers, int fd){
	int i;
	if (!work)
		if (aio_cancel(fd, NULL) == -1)
			error("Cannot cancel async. I/O operations");
	for (i = 0; i<BLOCKS; i++)
		free(buffers[i]);
	if (TEMP_FAILURE_RETRY(fsync(fd)) == -1)
		error("Error running fsync");
}
void reversebuffer(char *buffer, int blocksize){
	int k;
	char tmp;
	for (k = 0; work && k < blocksize / 2; k++){
		tmp = buffer[k];
		buffer[k] = buffer[blocksize - k - 1];
		buffer[blocksize - k - 1] = tmp;
	}
}
void processblocks(struct aiocb *aiocbs, char **buffer, int bcount, int bsize, int iterations){
	int curpos, j, index[2];
	iterations--;
	curpos = iterations == 0 ? 1 : 0;
	readdata(&aiocbs[1], bsize * (rand() % bcount));
	suspend(&aiocbs[1]);
	for (j = 0; work && j<iterations; j++){
		getindexes(index, bcount);
		if (j > 0) writedata(&aiocbs[curpos], index[0] * bsize);
		if (j < iterations-1) readdata(&aiocbs[SHIFT(curpos, 2)], index[1] * bsize);
		reversebuffer(buffer[SHIFT(curpos, 1)], bsize);
		if (j > 0) syncdata(&aiocbs[curpos]);
		if (j < iterations-1) suspend(&aiocbs[SHIFT(curpos, 2)]);
		curpos = SHIFT(curpos, 1);
	}
	if (iterations == 0) reversebuffer(buffer[curpos], bsize);
	writedata(&aiocbs[curpos], bsize * (rand() % bcount));
	suspend(&aiocbs[curpos]);
}
int main(int argc, char *argv[]){
	char *filename, *buffer[BLOCKS];
	int fd, n, k, blocksize, i;
	struct aiocb aiocbs[4];
	if (argc != 4)
		usage(argv[0]);
	filename = argv[1];
	n = atoi(argv[2]);
	k = atoi(argv[3]);
	if (n < 2 || k < 1)
		return EXIT_SUCCESS;
	work = 1;
	sethandler(siginthandler, SIGINT);
	if ((fd = TEMP_FAILURE_RETRY(open(filename, O_RDWR))) == -1)
		error("Cannot open file");
	blocksize = (getfilelength(fd) - 1) / n;
	fprintf(stderr, "Blocksize: %d\n", blocksize);
	if (blocksize > 0)
	{
		for (i = 0; i<BLOCKS; i++)
			if ((buffer[i] = (char *) calloc (blocksize, sizeof(char))) == NULL)
				error("Cannot allocate memory");
		fillaiostructs(aiocbs, buffer, fd, blocksize);
		srand(time(NULL));
		processblocks(aiocbs, buffer, n, blocksize, k);
		cleanup(buffer, fd);
	}
	if (TEMP_FAILURE_RETRY(close(fd)) == -1)
		error("Cannot close file");
	return EXIT_SUCCESS;
}
/*
Inclrease of BLOCKS value will not improve the algorithm, it will use only 3 blocks at a time and the rest will be just a waste of memory. Decrease of this value below 3 will turn the program useless.
Algorithms based on random decisions are easier to test if you fix the random seed. Instead of srand(time(NULL)) use srand(1) for testing. It will result in rand generating the same sequences of numbers every time you run the program.
Block size calculation is based on the file size decreased by one "blocksize = (getfilelength(fd) - 1) / n;". This decrease is a caused by the rule that UNIX text file should always end with new line character. As we do not want to move this last character in the file (\n) we must decrease the size by one. If you wish to use binary files (without last \n) remove - 1 from the calculation.
Call to function aio_fsync requires separate synchronization as regular aio_read and aio_write operations thus double suspend call in function syncdata - first suspend is for write operation, the second for disk synchronization.
Start with the analysis how the code runs for one iteration (k=1) and then for multiple iterations (k>0).
In what way 3 buffers and 3 aiocb's are utilised? What is stored in 3 structures starting at curpos?
Ad.Buffers (and structures) are used in cyclic way. To ease the calculation of next index in the array (with wrapping) macro SHIFT was introduced. The designation of buffers in a sequence starting at curpos at the beginning of the iteration is: (curpos) ready block to write in this iteration, (curpos+1) block read and ready for processing in this iteration, (curpos+2) block for read operation to be started in this iteration.
Try to describe what happens concurrently in the main loop, draw a time line whit time unit equal to one iteration, for each buffer (1,2,3) and each time slot (assume k=4) write what is done with the buffer. Possible notations are read (R), write (W), processing (P) and nothing (-). Please notice that the first read and the last write are done outside of the main loop, do not include them.
Ad.
0 1 2
- P R
R W P
P R W
W P -
Why AIO synchronization (aio_suspend) is not done immediately after the operation, instead it is postponed till the end of the iteration?
Ad.To achieve some concurrency of IO and buffer processing. The synchronization is done after the buffer reversing and in time before it, both IO and processing runs in parallel.
Are indexes of read block A and write block B always different?
Ad.No, for one iteration no check is made, for many iterations we only check if concurrent read and write blocks are at different positions but do notice that in one iteration we read the block we are going to process during next iteration. It may happen that the write position will be exactly the same as read position.
What is the meaning of different indexes condition in this task?
Ad.For this algorithm it is critical not to read and write the same block in parallel (in the same iteration) thus the condition on indexes. Without it, data in file may get corrupted. The problem does not affect one iteration as this is solved without real AIO.
When selcting values for indexes the second value is randomly selected from narrower range, the maximum is decreased by one, why?
Ad.This trick helps us to keep code shorter, if indexes are equal you always can increase the second one because it was at most maximum minus one. Without it you must code the case when indexes are equal and maximal at the same time.
Why in first iteration of the main loop we skip write and in the last one we skip read operation?
Ad.In first iteration we do not have ready buffer to write in the last one we do not need another read because we will not write it anywhere, we are done with iterations.
Is it necessary to use AIO for the first read and the last write in the program?
Ad.No, those operations must be immediately synchronized - regular read and write can be used instead.
When AIO cancellation takes place, is it correctly coded?
Ad.It is initiated in cleanup function provide the process received the SIGINT signal (we know about it from global work variable). After the cancellation this code is not checking if the cancellation was fully successful (AIO_CANCELED) or fully failed (AIO_ALLDONE) or partially done (AIO_NOTCANCELED). In the last case the program should wait for the operations still in progress before it can release the buffers' memory.
Correct the code for exercise.
Convert all AIO operations to synchronous IO (change all aio_ calls to synchronous IO calls and get rid of aio_suspend). Do some testing for small blocks (10B) and large blocks (2MB). To create large random file you can run this "$dd bs=1024 count=200000 if=/dev/urandom of=testBin.txt". When AIO is faster, can it be slower that regular IO? To measure time you can use time command ($ man time).
Ad.In my tests small blocks were processed in comparable times, this proves the Linux implementation of AIO to be quite fast, I expected it to be slower. Processing of large blocks was more than two times faster with AIO. I tested for 100 iterations.
