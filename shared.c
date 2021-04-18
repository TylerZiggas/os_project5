#include "oss.h"

void createFile(char* path) { // Creation of file
	FILE* newFile = fopen(path, "w"); // Take whatever char* was passed
		if (newFile == NULL) { // If there is a problem creating the file
			perror("Failed to touch file");
			exit(EXIT_FAILURE);
		}
	fclose(newFile); // Close file at end regardless
}

int linesInFile = 0;

void logOutput(char* path, char* fmt, ...) {
	if (linesInFile < 100000) {
		linesInFile++;
		FILE* fp = fopen(path, "a+"); // Open a file for writing
	
		if (fp == NULL) { // In case of failed logging to file
			perror("Failed to open file for logging output");
			exit(EXIT_FAILURE);
		}
		
		int n = 4096; 
		char buf[n];
		va_list args; // Intialize to grab all arguments for logging
		va_start(args, fmt);
		vsprintf(buf, fmt, args);
		va_end(args);	
		fprintf(fp, buf); // Writing to the file 
		fclose(fp);
	} 
}

int randomNumber(int min, int max) { // Function for creating random numbers
	srand(time(NULL));
	return rand()%(max - min) + min;
}

long int convertNano(int q) { // Function for converting to nanoseconds so we can print
	return q * 1000000000;

}

int convertMillis(int q) { // FUnction for converting to milliseconds so we can print
    return q / 1000000;
}

void addTime(struct time* time, int seconds, int nanoseconds) { // Adding to the time
        time->seconds += seconds;
        time->nanoseconds += nanoseconds;
        while(time->nanoseconds >= 1000000000) { // Checking to correct the time from nanoseconds to secounds
                time->nanoseconds -=1000000000;
                time->seconds++;
        }
}

char* getFormattedTime() { // Creation of formatted time, mostly for log file
        char* formattedTime = malloc(FORMATTED_TIME_SIZE * sizeof(char)); // allocate memory for it
        time_t now = time(NULL);
        strftime(formattedTime, FORMATTED_TIME_SIZE, FORMATTED_TIME_FORMAT, localtime(&now)); // format time we just recieved
        return formattedTime;
}

// Queue Block //

struct Queue* createQueue(int capacity) { // Creation of queues
	struct Queue* queue = (struct Queue*) malloc(sizeof(struct Queue)); // Allocate the queue
	queue->capacity = capacity;
	queue->front = queue->size = 0;
	queue->rear = capacity - 1;
	queue->array = (int*)malloc(queue->capacity*sizeof(int));
	return queue;
}

int full(struct Queue* queue) { // Checking to see if the queue is full
	if(queue->size == queue->capacity) { // if the queue is full
		return 1;
	} else { // if the queue is empty
		return 0;
	}
}

int empty(struct Queue* queue) { // check if the queue is empty
	return(queue->size == 0);
}

int queueSize(struct Queue* queue) { // check the size of the queue
	return queue->size;
}

void enqueue(struct Queue* queue, int id) { // Putting into the queue
	if(full(queue) == 1) {
		return;
	}

	queue->rear = (queue->rear + 1)%queue->capacity;
	queue->array[queue->rear] = id;
	queue->size = queue->size + 1; // Increase the size of queue

}

int dequeue(struct Queue* queue) { // removing from the queue
	if(empty(queue)) { 
		return INT_MIN;
	}

	int id = queue->array[queue->front]; 
	queue->front = (queue->front + 1) % queue->capacity;
	queue->size = queue->size - 1; // Decrease the size of queue
	return id;
}

// Queue Block //
