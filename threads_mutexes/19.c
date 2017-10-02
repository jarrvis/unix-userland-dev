#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#define MAXLINE 4096
#define DEFAULT_ARRAYSIZE 10
#define DELETED_ITEM -1
#define ERR(source) (perror(source),\
		     fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		     exit(EXIT_FAILURE))

typedef struct argsSignalHandler {
	pthread_t tid;
	int *pArrayCount;
	int *array;
	pthread_mutex_t *pmxArray;
	sigset_t *pMask;
	bool *pQuitFlag;
	pthread_mutex_t *pmxQuitFlag;
} argsSignalHandler_t;

void ReadArguments(int argc, char** argv, int *arraySize);
void removeItem(int *array, int *arrayCount, int index);
void printArray(int *array, int arraySize);
void* signal_handling(void*);

int main(int argc, char** argv) {
	int arraySize,*array;
	bool quitFlag = false;
	pthread_mutex_t mxQuitFlag = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_t mxArray = PTHREAD_MUTEX_INITIALIZER;
	ReadArguments(argc, argv, &arraySize);
	int arrayCount = arraySize;
	if(NULL==(array = (int*) malloc(sizeof(int) * arraySize)))ERR("Malloc error for array!");
	for (int i =0; i < arraySize; i++) array[i] = i + 1;
	sigset_t oldMask, newMask;
	sigemptyset(&newMask);
	sigaddset(&newMask, SIGINT);
	sigaddset(&newMask, SIGQUIT);
	if (pthread_sigmask(SIG_BLOCK, &newMask, &oldMask)) ERR("SIG_BLOCK error");
	argsSignalHandler_t args;
	args.pArrayCount = &arrayCount;
	args.array = array;
	args.pmxArray = &mxArray;
	args.pMask = &newMask;
	args.pQuitFlag = &quitFlag;
	args.pmxQuitFlag = &mxQuitFlag;
	if(pthread_create(&args.tid, NULL, signal_handling, &args))ERR("Couldn't create signal handling thread!");
	while (true) {
		pthread_mutex_lock(&mxQuitFlag);
		if (quitFlag == true) {
			pthread_mutex_unlock(&mxQuitFlag);
			break;
		} else {
			pthread_mutex_unlock(&mxQuitFlag);
			pthread_mutex_lock(&mxArray);
			printArray(array, arraySize);
			pthread_mutex_unlock(&mxArray);
			sleep(1);
		}
	}
	if(pthread_join(args.tid, NULL)) ERR("Can't join with 'signal handling' thread");
	free(array);
	if (pthread_sigmask(SIG_UNBLOCK, &newMask, &oldMask)) ERR("SIG_BLOCK error");
	exit(EXIT_SUCCESS);
}

void ReadArguments(int argc, char** argv, int *arraySize)
{
	*arraySize = DEFAULT_ARRAYSIZE;

	if (argc >= 2) {
		*arraySize = atoi(argv[1]);
		if (*arraySize <= 0) {
			printf("Invalid value for 'array size'");
			exit(EXIT_FAILURE);
		}
	}
}

void removeItem(int *array, int *arrayCount, int index) {
	int curIndex = -1;
	int i = -1;
	while (curIndex != index) {
		i++;
		if (array[i] != DELETED_ITEM)
			curIndex++;
	}
	array[i] = DELETED_ITEM;
	*arrayCount -= 1;
}

void printArray(int* array, int arraySize) {
	printf("[");
	for (int i =0; i < arraySize; i++)
		if (array[i] != DELETED_ITEM)
			printf(" %d", array[i]);
	printf(" ]\n");
}

void* signal_handling(void* voidArgs) {
	argsSignalHandler_t* args = voidArgs;
	int signo;
	srand(time(NULL));
	for (;;) {
		if(sigwait(args->pMask, &signo)) ERR("sigwait failed.");
		switch (signo) {
			case SIGINT:
				pthread_mutex_lock(args->pmxArray);
				if (*args->pArrayCount >  0)
					removeItem(args->array, args->pArrayCount, rand() % (*args->pArrayCount));
				pthread_mutex_unlock(args->pmxArray);
				break;
			case SIGQUIT:
				pthread_mutex_lock(args->pmxQuitFlag);
				*args->pQuitFlag = true;
				pthread_mutex_unlock(args->pmxQuitFlag);
				return NULL;
			default:
				printf("unexpected signal %d\n", signo);
				exit(1);
		}
	}
	return NULL;
}

/*Thread input structure argsSignalHandler_t holds the shared threads data (an array and STOP flag) with protective mutexes and not shared (signal mask and tid of thread designated to handle the signals).
In threaded process (one that has more that one thread) you can not use sigprocmask, use pthread_sigmask instead.
Having separated thread to handle the signals (as in this example) is a very common way to deal with signals in multi-threaded code.
How many threads run in this program?
Ad:Two, main thread created by system (every process has one starting thread) and the thread created by the code.
Name differences and similarities between sigwait i sigsuspend:
sigwait does not require signal handling routine as sigsuspend
both functions require blocking of the anticipated signal/signals
sigwait can not be interrupted by signal handling function (it is POSIX requirement), sigsuspend can
sigwait does not change the mask of blocked signals, even if signal handler is set it will not be triggered (in this example we do not have handlers), it will be executed on sigsuspend call
After successful call to sigwait only one type of pending signal is removed from the pending signals vector thus the problem we experienced with sigsuspend in L2 example can be corrected when using sigwait instead of sigsuspend. Please correct the program in L2 as an exercise.
Does the method of waiting for the end of working thread have the same flow as the method in previous example?
Ad:No, periodical printout of the table is a part of the task, busy waiting is when looping is coded only for waiting for the condition to become true. Despite that in this example we join the thread.
Can we use sigprocmask instead of pthread_sigmask in this program?
Ad:Yes, the signal blocking is set prior to thread creation, still in single thread phase of the program.
Why system calls to functions operating on mutex (acquire, release) are not tested for errors?
Ad:Basic mutex type (the type used in this program, default one) is not checking nor reporting errors. Adding those checks would not be such a bad idea as they are not harming the code and if you decide to later change the mutex type to error checking it will not require many changes in the code.
*/