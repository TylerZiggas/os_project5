// Author: Tyler Ziggas
// Date: April 2021
// user_proc.c is where the processes attempt to successfully allocate resources, processes can become 
// deadlocked as a result of processes attempting to grab the same resources, we kill those and log how the processes end,
// whether they end successfully or end prematurely/while deadlocked.

#include "oss.h"

struct{ // Structure for the messsage buffer
	long mtype;
	char msg[10];
} msgbuf;

struct sharedRes *shared;

int shmID, toChild, ossQueue; // Our keys
const int RELEASE_OR_REQUEST = 1000000; // Constant declarations
const int REQUEST_RESOURCE = 70;
const int DIE = 10;

int main(int argc, char *argv[]) {
	int pid = atoi(argv[0]);
	time_t t; 
	time(&t);	
	srand((int)time(&t) % getpid()); // Set up the randomness of this run

	key_t key;
	key = ftok(".",'a');
	if((shmID = shmget(key,sizeof(struct sharedRes),0666)) == -1) { // shmget for our shared memory
		perror("shmget");
		exit(EXIT_FAILURE);	
	}
	
	shared = (struct sharedRes*)shmat(shmID,(void*)0,0); // attaching of our shared memory
	if(shared == (void*)-1) {
		perror("Error on attaching memory");
		exit(EXIT_FAILURE);
	}

	key_t msgkey;
	if((msgkey = ftok("Makefile",925)) == -1) { // Creation of a message queue
		perror("ftok");
		exit(EXIT_FAILURE);
	}

	if((toChild = msgget(msgkey, 0600 | IPC_CREAT)) == -1) { // msgget for our first messsage queue
		perror("msgget");
		exit(EXIT_FAILURE);
	}	
	
	if((msgkey = ftok("Makefile",825)) == -1) { // Creation of another message queue
		perror("ftok");
		exit(EXIT_FAILURE);
	}

	if((ossQueue = msgget(msgkey, 0600 | IPC_CREAT)) == -1) { // msgget for our first message queue
		perror("msgget");	
		exit(EXIT_FAILURE);
	}

	int interval = (rand() % RELEASE_OR_REQUEST + 1);
	struct time actionClock; // Start setting up our clock for this process
	actionClock.seconds = shared->time.seconds;
	actionClock.nanoseconds = shared->time.nanoseconds;
	addTime(&actionClock, 0, interval);
	
	int termination = (rand() % (250 * 1000000) + 1); // Set a random number for termination
	struct time shouldTerminate; // Setting up our timer for when they should terminate
	shouldTerminate.seconds = shared->time.seconds;
	shouldTerminate.nanoseconds = shared->time.nanoseconds;
	addTime(&shouldTerminate, 0, termination);

	do {	
		if((shared->time.seconds > actionClock.seconds) || (shared->time.seconds == actionClock.seconds && shared->time.nanoseconds >= actionClock.nanoseconds)) {
			actionClock.seconds = shared->time.seconds; // rewriting the clock
			actionClock.nanoseconds = shared->time.nanoseconds;
			addTime(&actionClock, 0, interval);	
	
			if((rand() % 100) < REQUEST_RESOURCE) {	// Randomly decide if it will request
				strcpy(msgbuf.msg,"REQUEST");
				msgbuf.mtype = pid; // Grab our pid
				msgsnd(ossQueue,&msgbuf,sizeof(msgbuf),0);

				int resourceToRequest = (rand() % 20);
				sprintf(msgbuf.msg, "%d", resourceToRequest); // Write our new message
				msgsnd(ossQueue, &msgbuf,sizeof(msgbuf),0); // Send the message

				int instances = (rand()% (shared->resources[resourceToRequest].instances)) + 1;
				
				sprintf(msgbuf.msg, "%d", instances);
				msgsnd(ossQueue, &msgbuf,sizeof(msgbuf),0);
				while(1) {
					msgrcv(toChild,&msgbuf,sizeof(msgbuf),pid,0); 
					if(strcmp(msgbuf.msg, "GRANTED") == 0) { // Check if we are granting a resource
						break;
					}
					if(strcmp(msgbuf.msg, "TERM") == 0) { // If we are suppose to terminate, exit the process
						exit(EXIT_SUCCESS);
					}
				}
			} else { // Otherwise we are trying to release 
				strcpy(msgbuf.msg,"RELEASE");
				msgbuf.mtype = pid; // Grab our pid
				msgsnd(ossQueue,&msgbuf,sizeof(msgbuf),0);
				int i, resource = -1;
				
				for(i = 0; i < 20; i++) { // Allocate for the resouces and break
					if(shared->resources[i].allocated[pid - 1] > 0){
						resource = i;
						break; 
					}	
				}
				sprintf(msgbuf.msg, "%d", resource);
				msgsnd(ossQueue,&msgbuf, sizeof(msgbuf),0); // Send the message of what we just did
			}
		}

		if((shared->time.seconds > shouldTerminate.seconds) || (shared->time.seconds == shouldTerminate.seconds && shared->time.nanoseconds >= shouldTerminate.nanoseconds)){
			termination = (rand() % (250 * 1000000) + 1); // Set up the termination
			shouldTerminate.seconds = shared->time.seconds;
			shouldTerminate.nanoseconds = shared->time.nanoseconds;
			addTime(&shouldTerminate, 0, termination); // Add time to the timer
			if((rand()%100) <= DIE) { // Decide whether or not it is time for the process to die
				strcpy(msgbuf.msg,"TERMINATED");
				msgbuf.mtype = pid;
				msgsnd(ossQueue,&msgbuf,sizeof(msgbuf),0);	
				exit(0);
			}
		}
	} while(1); // We want to loop here 
	exit(EXIT_SUCCESS); // End if we get out somehow
}
