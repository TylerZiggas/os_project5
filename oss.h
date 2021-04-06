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

#define FORMATTED_TIME_SIZE 50
#define FORMATTED_TIME_FORMAT "%H:%M:%S"

void createFile(char*);
void logOutput(char*, char*, ...);
int randomNumber(int, int);
long int convertNano(int);
int convertMillis(int);
char* getFormattedTime();
void sigact(int, void(int));
void signalHandler(int);
void setupTimer(const int t);

#endif
