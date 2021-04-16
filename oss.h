#pragma once
#ifndef OSS_H
#define OSS_H

#include <stdlib.h>
#include <getopt.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <math.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/sem.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <time.h>
#include<sys/msg.h>
#include <limits.h>

#define FORMATTED_TIME_SIZE 50
#define FORMATTED_TIME_FORMAT "%H:%M:%S"

struct time{
	int nanoseconds;
	int seconds;
};

struct resource_descriptor{
	int request[18];
	int release[18];
	int allocated[18];
	int shareable;
	int instances;
	int available;
};


struct sharedRes{
	struct resource_descriptor resources[20];
	struct time time;
};

struct Queue{
	int front,rear,size;
	int capacity;
	int *array;
};

struct Queue* createQueue(int capacity);

void clean_resources();
void add_clock(struct time* time, int sec, int ns);
void oss_exit_signal(int sig);
int get_free_slot_id();
int allocate_resource(int resourceId,int pid);
void try_release_resource(int pid,int verbose);
void run_simulation(int verbose);
void deallocateResource(int resourceId, int pid);

void createFile(char*);
void logOutput(char*, char*, ...);
int randomNumber(int, int);
long int convertNano(int);
int convertMillis(int);
char* getFormattedTime();
void sigact(int, void(int));
void signalHandler(int);
void setupTimer(const int t);

int queueSize(struct Queue* queue);
int full(struct Queue* queue);
int empty(struct Queue* queue);
void enqueue(struct Queue* queue,int controlBlock);
int dequeue(struct Queue* queue);
int front(struct Queue* queue);
int back (struct Queue* queue);


#endif
