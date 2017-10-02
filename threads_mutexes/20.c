#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>

#define MAXLINE 4096
#define DEFAULT_STUDENT_COUNT 100
#define ELAPSED(start,end) ((end).tv_sec-(start).tv_sec)+(((end).tv_nsec - (start).tv_nsec) * 1.0e-9)
#define ERR(source) (perror(source),\
		     fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		     exit(EXIT_FAILURE))

typedef unsigned int UINT;
typedef struct timespec timespec_t;
typedef struct studentList {
	bool *removed;
	pthread_t *thStudents;
	int count;
	int present;
} studentsList_t;
typedef struct yearCounters {
	int values[4];
	pthread_mutex_t mxCounters[4];
} yearCounters_t;
typedef struct argsModify {
	yearCounters_t *pYearCounters;
	int year;
} argsModify_t;
void ReadArguments(int argc, char** argv, int *studentsCount);
void* student_life(void*);
void increment_counter(argsModify_t *args);
void decrement_counter(argsModify_t *args);
void msleep(UINT milisec);
void kick_student(studentsList_t *studentsList);

int main(int argc, char** argv) {
	int studentsCount;
	ReadArguments(argc, argv, &studentsCount);
	yearCounters_t counters = {
		.values = { 0, 0, 0, 0 },
		.mxCounters = {
				PTHREAD_MUTEX_INITIALIZER,
				PTHREAD_MUTEX_INITIALIZER,
				PTHREAD_MUTEX_INITIALIZER,
				PTHREAD_MUTEX_INITIALIZER}
	};
	studentsList_t studentsList;
	studentsList.count = studentsCount;
	studentsList.present = studentsCount;
	studentsList.thStudents = (pthread_t*) malloc(sizeof(pthread_t) * studentsCount);
	studentsList.removed = (bool*) malloc(sizeof(bool) * studentsCount);
	if (studentsList.thStudents == NULL || studentsList.removed == NULL) 
		ERR("Failed to allocate memory for 'students list'!");
	for (int i = 0; i < studentsCount; i++) studentsList.removed[i] = false;
	for (int i = 0; i < studentsCount; i++)
		if(pthread_create(&studentsList.thStudents[i], NULL, student_life, &counters)) ERR("Failed to create student thread!");
	srand(time(NULL));
	timespec_t start, current;
	if (clock_gettime(CLOCK_REALTIME, &start)) ERR("Failed to retrieve time!");
	do {
		msleep(rand() % 201 + 100);
		if (clock_gettime(CLOCK_REALTIME, &current)) ERR("Failed to retrieve time!");
		kick_student(&studentsList);
	}
	while (ELAPSED(start, current) < 4.0);
	for (int i = 0; i < studentsCount; i++) 
		if(pthread_join(studentsList.thStudents[i], NULL)) ERR("Failed to join with a student thread!");
	printf(" First year: %d\n", counters.values[0]);
	printf("Second year: %d\n", counters.values[1]);
	printf(" Third year: %d\n", counters.values[2]);
	printf("  Engineers: %d\n", counters.values[3]);
	free(studentsList.removed);
	free(studentsList.thStudents);
	exit(EXIT_SUCCESS);
}

void ReadArguments(int argc, char** argv, int *studentsCount) {
	*studentsCount = DEFAULT_STUDENT_COUNT;
	if (argc >= 2) {
		*studentsCount = atoi(argv[1]);
		if (*studentsCount <= 0) {
			printf("Invalid value for 'studentsCount'");
			exit(EXIT_FAILURE);
		}
	}
}

void* student_life(void *voidArgs) {
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
	argsModify_t args;
	args.pYearCounters = voidArgs;
	for(args.year = 0;args.year < 3;args.year++){
		increment_counter(&args);
		pthread_cleanup_push(decrement_counter, &args);
		msleep(1000);
		pthread_cleanup_pop(1);
	}
	increment_counter(&args);
	return NULL;
}

void increment_counter(argsModify_t *args) {
	pthread_mutex_lock(&(args->pYearCounters->mxCounters[args->year]));
	args->pYearCounters->values[args->year] += 1;
	pthread_mutex_unlock(&(args->pYearCounters->mxCounters[args->year]));
}

void decrement_counter(argsModify_t *args) {
	pthread_mutex_lock(&(args->pYearCounters->mxCounters[args->year]));
	args->pYearCounters->values[args->year] -= 1;
	pthread_mutex_unlock(&(args->pYearCounters->mxCounters[args->year]));
}

void msleep(UINT milisec) {
    time_t sec= (int)(milisec/1000);
    milisec = milisec - (sec*1000);
    timespec_t req= {0};
    req.tv_sec = sec;
    req.tv_nsec = milisec * 1000000L;
    if(nanosleep(&req,&req)) ERR("nanosleep");
}

void kick_student(studentsList_t *studentsList) {
	int idx;
	if(0==studentsList->present) return;
	do {
		idx = rand() % studentsList->count;
	}
	while(studentsList->removed[idx] == true);
	pthread_cancel(studentsList->thStudents[idx]);
	studentsList->removed[idx] = true;
	studentsList->present--;
}

/*
Threads receive the pointer to the structure with current year and pointer to years counters, structure argsModify_t does not have the same flow as one in task 2 of this tutorial i.e. program is not making too many unnecessary references to the same data.
Structure studentsList_t is only used im main thread, it is not visible for students' threads.
Cleaver structure yearCounters_t initialization will not work in archaic C standards (c89/c90). It is worth knowing but please use all the improvements of newer standards in your code.
Cleanup handlers in working thread are deployed to safely expel the student while its thread is asleep. Without the handlers the canceled student will occupy the last counter till the end of the program!
Please keep in mind that pthread_cleanup_push must be paired with pthread_cleanup_pop in the same lexical scope (the same braces {}).
How many mutexes this program uses and what they protect?
Ad:Four exactly, one for each year/counter.
Must current year of a student be a part of argsModify_t structure?
Ad:No it could have been automatic in thread variable, then structure argsModify_t would be no longer needed and you would pass pointer to yearCounters instead.
What does it mean that the thread cancellation state is set to PTHREAD_CANCEL_DEFERRED ?
Ad:Cancellation will only happen during certain function calls (so called cancellation points), it will not disturb the rest of the code in the thread. In other words the thread can finish a part of its work before it is canceled.
What functions used in the thread code are cancellation points?
Ad: Only nanosleep (called from msleep) is a cancellation point in this code.
How do we learn what functions are cancellation points?
Ad:$man 7 pthreads
What one in this call " pthread_cleanup_pop(1);" means ?
Ad:It means that the handler is not only removed from the handlers stack but also executed.
When the year counter is decreased?
Ad:In two cases, during cancellation (rare case), during the removal of cleanup handler from the stack of handlers.
Algorithm selecting a thread for cancellation has an apparent flow, can you name it and tell what threat it creates?
Ad:This random selection can last very long if only a few "live" threads are left on a large list of threads. Try to run the program with 10 as the parameter to check it.
Improve random selection as an exercise.
Have a look at the method used to measure the 4 seconds life time of the program (clock_gettime, nanosleep). Please change the solution to use alarm function and the SIGALRM handler as an exercise.
*/
