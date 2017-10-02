#include <stdlib.h>
#include <stddef.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#define MAXLINE 4096
#define DEFAULT_THREADCOUNT 10
#define DEFAULT_SAMPLESIZE 100

#define ERR(source) (perror(source),\
		     fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		     exit(EXIT_FAILURE))

typedef unsigned int UINT;
typedef struct argsEstimation {
	pthread_t tid;
	UINT seed;
	int samplesCount;
} argsEstimation_t;

void ReadArguments(int argc, char **argv, int *threadCount, int *samplesCount);
void* pi_estimation(void *args);

int main(int argc, char** argv) {
	int threadCount, samplesCount;
	double *subresult;
	ReadArguments(argc, argv, &threadCount, &samplesCount);
	argsEstimation_t* estimations = (argsEstimation_t*) malloc(sizeof(argsEstimation_t) * threadCount);
	if (estimations == NULL) ERR("Malloc error for estimation arguments!");
	srand(time(NULL));
	for (int i = 0; i < threadCount; i++) {
		estimations[i].seed = rand();
		estimations[i].samplesCount = samplesCount;
	}
	for (int i = 0; i < threadCount; i++) {
		int err = pthread_create(&(estimations[i].tid), NULL, pi_estimation, &estimations[i]);
		if (err != 0) ERR("Couldn't create thread");
	}
	double cumulativeResult = 0.0;
	for (int i = 0; i < threadCount; i++) {
		int err = pthread_join(estimations[i].tid, (void*)&subresult);
		if (err != 0) ERR("Can't join with a thread");
		if(NULL!=subresult){
			cumulativeResult += *subresult;
			free(subresult);
		}
	}
	double result = cumulativeResult / threadCount;
	printf("PI ~= %f\n", result);
	free(estimations);
}

void ReadArguments(int argc, char **argv, int *threadCount, int *samplesCount) {
	*threadCount = DEFAULT_THREADCOUNT;
	*samplesCount = DEFAULT_SAMPLESIZE;

	if (argc >= 2) {
		*threadCount = atoi(argv[1]);
		if (*threadCount <= 0) {
			printf("Invalid value for 'threadCount'");
			exit(EXIT_FAILURE);
		}
	}
	if (argc >= 3) {
		*samplesCount = atoi(argv[2]);
		if (*samplesCount <= 0) {
			printf("Invalid value for 'samplesCount'");
			exit(EXIT_FAILURE);
		}
	}
}

void* pi_estimation(void *voidPtr) {
	argsEstimation_t *args = voidPtr;
	double* result;
	if(NULL==(result=malloc(sizeof(double)))) ERR("malloc");;

	int insideCount = 0;
	for (int i = 0; i < args->samplesCount; i++) {
		double x = ((double) rand_r(&args->seed) / (double) RAND_MAX);
		double y = ((double) rand_r(&args->seed) / (double) RAND_MAX);
		if (sqrt(x*x+y*y) <= 1.0) insideCount ++;
	}
	*result = 4.0 * (double) insideCount / (double) args->samplesCount;
	return result;
}
/*
This and following programs do not show USAGE information, the default parameters values are assumed if options are missing. Run it without parameters to see how it works.
Functions' declarations at the beginning of the code (not the functions definitions) are quite useful, sometimes mandatory. If you do not know the difference please read this.
In multi threaded processes you can not correctly use rand() function, use rand_r() instead. The later one requires individual seed for every thread.
This program uses the simplest schema for threads lifetime. It creates some threads and then immediately waits for them to finish. More complex scenarios are possible
Please keep in mind that nearly every call to system function (and most calls to library functions) should be followed with the test on errors and if necessary by the proper reaction on the error.
ERR macro does not send "kill" signal as in multi-process program, why ?
Ad:Call to exit terminates all the threads in current process, there is no need to "kill" other processes.
How input data is passed to the new threads?
Ad:Exclusively by the pointer to structure argsEstimation_t that is passed as thread function arguments. There is no need (nor the excuse) to use global variables!
Is the thread input data shared between the threads?
Ad:Not in this program. In this case there is no need to synchronize the access to this data. Each thread gets a pointer to the private copy of the structure.
How the random seed for rand_r() is prepared for each thread?
Ad:It is randomized in the main thread and passed as a part of input data in argsEstimation_t .
In this multi thread program we see srand/rand calls, is this correct? It contradicts what was said a few lines above.
Ad:Only one thread is using srand/rand and those functions are called before working threads come to life. The problem with srand/rand and threads originates from one global variable used in the library to hold the current seed. In this code only on thread access this seed thus it is correct.
Can we share one input data structure for all the threads instead of having a copy for every thread?
Ad:No due to random seed, it must be different for all the threads.
Can we make the array with the thread input data automatic variable (not allocated)? 
Ad:Only if we add some limit on the number of working threads (up to 1000) otherwise this array may use all the stack of the main thread.
Why do we need to release the memory returned from the working thread?
Ad:This memory was allocated in the thread and has to be released somewhere in the same process, The heap is sheared by all the threads if you do not release it you will leak the memory. It will not be released automatically on the thread termination.
Why can't we return the data as the address of local (to the thread) automatic variable? 
Ad:The moment thread terminates is the moment of its stack memory release. If you have a pointer to this released stack you should not use it as this memory can be overwritten immediately. What worse, in most cases this memory will stil be the same and faulty program will work in 90% of cases. If you make this kind of mistake it is later very hard to find out why sometimes your code fails. Please be careful and try to avoid this flaw.
can we avoid memory allocation in the working thread?
Ad:Yes, if we add extra variable to the input structure of the thread. The result can then be stored in this variable.
*/