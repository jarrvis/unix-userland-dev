#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define MAXLINE 4096
#define DEFAULT_N 1000
#define DEFAULT_K 10
#define BIN_COUNT 11
#define NEXT_DOUBLE(seedptr) ((double) rand_r(seedptr) / (double) RAND_MAX)
#define ERR(source) (perror(source),\
		     fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		     exit(EXIT_FAILURE))

typedef unsigned int UINT;
typedef struct argsThrower{
	pthread_t tid;
	UINT seed;
	int *pBallsThrown;
	int *pBallsWaiting;
	int *bins;
	pthread_mutex_t *mxBins;
	pthread_mutex_t *pmxBallsThrown;
	pthread_mutex_t *pmxBallsWaiting;
} argsThrower_t;

void ReadArguments(int argc, char** argv, int *ballsCount, int *throwersCount);
void make_throwers(argsThrower_t *argsArray, int throwersCount);
void* throwing_func(void* args);
int throwBall(UINT* seedptr);

int main(int argc, char** argv) {
	int ballsCount, throwersCount;
	ReadArguments(argc, argv, &ballsCount, &throwersCount);
	int ballsThrown = 0, bt=0;
	int ballsWaiting = ballsCount;
	pthread_mutex_t mxBallsThrown = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_t mxBallsWaiting = PTHREAD_MUTEX_INITIALIZER;
	int bins[BIN_COUNT];
	pthread_mutex_t mxBins[BIN_COUNT];
	for (int i =0; i < BIN_COUNT; i++) {
		bins[i] = 0;
		if(pthread_mutex_init(&mxBins[i], NULL))ERR("Couldn't initialize mutex!");
	}
	argsThrower_t* args = (argsThrower_t*) malloc(sizeof(argsThrower_t) * throwersCount);
	if (args == NULL) ERR("Malloc error for throwers arguments!");
	srand(time(NULL));
	for (int i = 0; i < throwersCount; i++) {
		args[i].seed = (UINT) rand();
		args[i].pBallsThrown = &ballsThrown;
		args[i].pBallsWaiting = &ballsWaiting;
		args[i].bins = bins;
		args[i].pmxBallsThrown = &mxBallsThrown;
		args[i].pmxBallsWaiting = &mxBallsWaiting;
		args[i].mxBins = mxBins;
	}
	make_throwers(args, throwersCount);
	while (bt<ballsCount) {
		sleep(1);
		pthread_mutex_lock(&mxBallsThrown);
		bt = ballsThrown;
		pthread_mutex_unlock(&mxBallsThrown);
	}
	int realBallsCount = 0;
	double meanValue = 0.0;
	for (int i =0 ; i < BIN_COUNT; i++) {
		realBallsCount += bins[i];
		meanValue += bins[i] * i;
	}
	meanValue = meanValue / realBallsCount;
	printf("Bins count:\n");
	for (int i = 0; i < BIN_COUNT; i++) printf("%d\t", bins[i]);
	printf("\nTotal balls count : %d\nMean value: %f\n", realBallsCount, meanValue);
	free(args);
	for (int i = 0; i < BIN_COUNT; i++) pthread_mutex_destroy(&mxBins[i]);
	exit(EXIT_SUCCESS);
}

void ReadArguments(int argc, char** argv, int *ballsCount, int *throwersCount) {
	*ballsCount = DEFAULT_N;
	*throwersCount = DEFAULT_K;
	if (argc >= 2) {
		*ballsCount = atoi(argv[1]);
		if (*ballsCount <= 0) {
			printf("Invalid value for 'balls count'");
			exit(EXIT_FAILURE);
		}
	}
	if (argc >= 3) {
		*throwersCount = atoi(argv[2]);
		if (*throwersCount <= 0) {
			printf("Invalid value for 'throwers count'");
			exit(EXIT_FAILURE);
		}
	}
}

void make_throwers(argsThrower_t *argsArray, int throwersCount) {
	pthread_attr_t threadAttr;
	if(pthread_attr_init(&threadAttr)) ERR("Couldn't create pthread_attr_t");
	if(pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_DETACHED)) ERR("Couldn't setdetachsatate on pthread_attr_t");
	for (int i = 0; i < throwersCount; i++) {
		if(pthread_create(&argsArray[i].tid, &threadAttr, throwing_func, &argsArray[i])) ERR("Couldn't create thread");
	}
	pthread_attr_destroy(&threadAttr);
}

void* throwing_func(void* voidArgs) {
	argsThrower_t* args = voidArgs;
	while (1) {
		pthread_mutex_lock(args->pmxBallsWaiting);
		if (*args->pBallsWaiting > 0) {
			(*args->pBallsWaiting) -= 1;
			pthread_mutex_unlock(args->pmxBallsWaiting);
		} else {
			pthread_mutex_unlock(args->pmxBallsWaiting);
			break;
		}
		int binno = throwBall(&args->seed);
		pthread_mutex_lock(&args->mxBins[binno]);
		args->bins[binno] += 1;
		pthread_mutex_unlock(&args->mxBins[binno]);
		pthread_mutex_lock(args->pmxBallsThrown);
		(*args->pBallsThrown) += 1;
		pthread_mutex_unlock(args->pmxBallsThrown);
	}
	return NULL;
}

/* returns # of bin where ball has landed */
int throwBall(UINT* seedptr) {
	int result = 0;
	for (int i = 0; i < BIN_COUNT - 1; i++) 
		if (NEXT_DOUBLE(seedptr) > 0.5) result++;
	return result;
}

/*
Once again all thread input data is passed as pointer to the structure (Thrower_t), treads results modify bins array (pointer in the same structure), no global variables used.
In this code two mutexes protect two counters and an array of mutexes protects the bins' array (one mutex for every cell in the array). In total we have BIN_COUNT+2 mutexes.
In this program we use detachable threads. There is now need (nor option) to wait for working threads to finish and thus lack of pthread_join. As we do not join the threads a different method must be deployed to test if the main program can exit.
In this example mutexes are created in two ways - as automatic and dynamic variables. The first method is simpler in coding but you need to know exactly how many mutexes you need at coding time. The dynamic creation requires more coding (initiation and removal) but the amount of mutexes also is dynamic.
Is data passed to threads in argsThrower_t structure shared between them?
Ad:Some of it, counters and bins are shared and thus protected with mutexes.
Is structure argsThrower_t optimal?
Ad:No - some fields point the same data for every thread. Common parts can be moved to additional structure and one pointer for this structure instead of 6 will be stored in argsThrower_t. Additionally we store tids in this structure while it is not used in the thread code.
Why do we mostly use pointers in the threads input data?
Ad: We share the data, without the pointers we would have independent copies of those variables in each thread.
Can we pass mutexes as variables? Not as pointers?
Ad:NO, POSIX FORBIDS, copy of a mutex does not have to be a working mutex! Even if it would work, it should be quite obvious that, a copy would be a different mutex.
This program uses a lot of mutexes, can we reduce the number of them?
Ad:Yes, in extreme case it can be reduced to only one mutex but at the cost of concurrency. In more reasonable approach you can have 2 mutexes for the counters and one for all the bins, although the concurrency is lower in this case the running time of a program can be a bit shorter as operations on mutexes are quite time consuming for the OS.
To check if the working threads terminated, the main threads periodically checks if the numer of thrown beans is equal to the number of beans in total. Is this optimal solution?
Ad:No, it is so called "soft busy waiting" but without synchronization tool like conditional variable it can not be solved better.
Do all the threads created in this program really work?
Ad:No ,especially when there is a lot of threads. It is possible that some of threads "starve". The work code for the thread is very fast, thread creation is rather slow, it is possible that last threads created will have no beans left to throw. To check it please add per thread thrown beans counters and print them on stdout at the thread termination. The problem can be avoided if we add synchronization on threads start - make them start at the same time but this again requires the methods that will be introduced during OPS2 (barier or conditional variable).
*/