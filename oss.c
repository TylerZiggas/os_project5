// Author: Tyler Ziggas
// Date: April 2021
// oss.c acts as the hub, helping to allocate resources and decide whether or not the resources can be allocated or if they are deadlocked
// This will check to see if you use -v to enable verbose logging, -h to remind the user that verbose logging exists, and a default that exists the program if we use something wrong.

#include "oss.h"

struct{
	long mtype;
	char msg[10];
} msgbuf; // Message buffer

struct sharedRes *shared;
struct Queue *waiting;

int toChild; // Integers for our queues and shared memory
int ossQueue;
int shmID;
int slots[18]; 
int reqGranted = 0; // Set up our counters for number of processes
int proTerminated = 0;
int proExited = 0;
int logTable = 1;
char* logfile = "logfile"; // Default logfile is logfile

int main(int argc, char *argv[]) {
	int character;
	bool verbose = false;
	createFile(logfile); // Immediately create the logfile
	while((character = getopt(argc, argv, "vh"))!= EOF) { // Set up the options that are allowed for cases
		switch(character) {
			case 'v': // Verbose logging
				verbose = true; // Turn on verbose logging
				printf("Verbose is on\n");
				break;
			case 'h': // Help to remind of verbose
				printf("This is invoked by ./oss and you may add -v after to turn on verbose logging.\n"); // Our help menu, only one line so I didn't make a new function
				exit(EXIT_SUCCESS);
			default:
				printf("That is not a valid parameter.\n"); // Default that exits the program
				exit(EXIT_FAILURE);
		}
	}
	srand(time(NULL)); // Set up our srand for the randomness of this program
	key_t key;
	key = ftok(".",'a');
	if((shmID = shmget(key, sizeof(struct sharedRes), IPC_CREAT | 0666)) == -1) { // shmget for our shared memory
		perror("shmget");
		exit(EXIT_FAILURE);
	}

	shared = (struct sharedRes*)shmat(shmID,(void*)0,0); // attaching of our shared memory
	if(shared == (void*)-1) {
		perror("Error attaching memory");
		exit(EXIT_FAILURE);
	}

	key_t msgkey;
	if((msgkey = ftok("Makefile",925)) == -1) { // Creation of our message queue
		perror("ftok");
		exit(EXIT_FAILURE);
	}

	if((toChild = msgget(msgkey, 0600 | IPC_CREAT)) == -1) { // msgget for our first message queue
		perror("msgget");
		exit(EXIT_FAILURE);
	}

	if((msgkey = ftok("Makefile",825)) == -1) { // Creation of another message queue
		perror("ftok");
		exit(EXIT_FAILURE);
	}

	if((ossQueue = msgget(msgkey, 0600 | IPC_CREAT)) == -1) { // msgget for our second message queue
		perror("msgget");
		exit(EXIT_FAILURE);
	}

	int i, j, index;
	for(i = 0; i < 20; i++) { // Creation of our dummy resources
		shared->resources[i].shareable = 0;
		int temp = (rand() % (10 - 1 + 1)) + 1;  // Random number for our resources
		shared->resources[i].instances = temp;
		shared->resources[i].available = temp;
		for(j = 0; j <= 18; j++) { // Intialize everything to 0
			shared->resources[i].request[j] = 0;
			shared->resources[i].release[j] = 0;
			shared->resources[i].allocated[j] = 0;
		}
	}
	int randShareable = (rand() % (5 - 1 + 1)) + 1; // Create the random number of shareable process resources
	for(i = 0; i < randShareable; i++) { // Start setting them up
		index = (rand() % (19 - 0 + 1 )) + 0;
		shared->resources[index].shareable = 0;
	}

	printf("Starting OSS simulation...\n");

	signal(SIGALRM, exitSignal); // Setting up of our alarms
	alarm(5);
	signal(SIGINT, exitSignal);

	simulateOSS(verbose); // Simulate resource management and pass in whether or not we use verbose logging
	cleanResources(shmID, shared); // Clean everything once we are done
	exit(EXIT_SUCCESS);
} 

void simulateOSS(bool verbose) {
	int i = 0, activeChild = 0, count = 0;
	struct time forkClock; // Creation of our clocks
	struct time deadlockClock;
	deadlockClock.seconds = 1; 

	int nextFork = (rand() % (500000000 - 1000000 + 1)) + 1000000; // Randomly get the time for our next fork
	addClock(&forkClock,0,nextFork); // Add time to the clock for how long we "wait"

	pid_t pids[18]; 
	waiting = createQueue(100);
	do {
		addClock(&shared->time,0,10000); // Add time to our clock
		if((activeChild < 18) && ((shared->time.seconds > forkClock.seconds) || (shared->time.seconds == forkClock.seconds && shared->time.nanoseconds >= forkClock.nanoseconds))) {
			forkClock.seconds = shared->time.seconds; // Set the amount of time for the forkclock wait
			forkClock.nanoseconds = shared->time.nanoseconds;
			nextFork = (rand() % (500000000 - 1000000 + 1)) + 1000000; // Randomly decide the time for the next fork
			addClock(&forkClock,0,nextFork);
			int newProc = freeID() + 1;
			if((newProc-1 > -1)) {
				pids[newProc - 1] = fork(); // Forking the product and adding the pid
				if(pids[newProc - 1] == 0){
					char str[10];
					sprintf(str, "%d", newProc);
					execl("./user_proc", str, NULL); // Sending the process with its pid
					exit(EXIT_SUCCESS);
				}
				activeChild++; // Add to the number of active children
				printf("Master spawned Process P%d at time %d:%d\n", newProc,shared->time.seconds,shared->time.nanoseconds);
				logOutput(logfile,"Master spawned Process P%d at time %d:%d\n",newProc,shared->time.seconds,shared->time.nanoseconds);
			}
		}

		if(msgrcv(ossQueue, &msgbuf, sizeof(msgbuf),0,IPC_NOWAIT) > -1) {
			int pid = msgbuf.mtype;
			if (strcmp(msgbuf.msg, "TERMINATED") == 0) { // Check to the see if the process is suppose to terminate
				while(waitpid(pids[pid - 1],NULL, 0) > 0);
				int m;
				proExited++; // Add to the list of processes that have terminated
				for(m = 0; m < 20; m++) {
					if(shared->resources[m].shareable == 0) { // Check if the process is shareable
						if(shared->resources[m].allocated[pid -1] > 0) {
							shared->resources[m].available += shared->resources[m].allocated[pid - 1]; // Add to the list of avilable
						}
					}
					shared->resources[m].allocated[pid - 1] = 0; // Reset the allocate and requests
					shared->resources[m].request[pid - 1] = 0;
				}
				activeChild--; // Subtract from the number of active children
				slots[pid - 1] = 0;
				count++; 
				printf("Process P%d terminated at time %d:%d\n",pid,shared->time.seconds,shared->time.nanoseconds);
				logOutput(logfile, "Master: Process P%d terminated at time %d:%d\n",pid,shared->time.seconds,shared->time.nanoseconds);

			} else if (strcmp(msgbuf.msg, "RELEASE") == 0) { // Check whether or not we are releasing anything
				msgrcv(ossQueue, &msgbuf,sizeof(msgbuf),pid,0);
				int releasedRes = atoi(msgbuf.msg);
				if(releasedRes > -1) { // We are to deallocate resources
					deallocateResource(releasedRes,pid);
					if(verbose == true) { // Write if verbose is on
						logOutput(logfile,"Master: Process P%d released resource R%d at time %d:%d\n",pid,releasedRes,shared->time.seconds,shared->time.nanoseconds);
					}
				}
			} else if (strcmp(msgbuf.msg, "REQUEST") == 0) { // Check whether or not we are requesting anything
				msgrcv(ossQueue, &msgbuf, sizeof(msgbuf),pid,0);
				int requestedRes = atoi(msgbuf.msg);

				if(verbose == true) { // Write if verbose is on
					logOutput(logfile, "Master: Process P%d requesting R%d at time %d:%d\n",pid,requestedRes,shared->time.seconds,shared->time.nanoseconds);
				}

				msgrcv(ossQueue, &msgbuf, sizeof(msgbuf),pid,0);
				int instances = atoi(msgbuf.msg);

				shared->resources[requestedRes].request[pid - 1] = instances;

				if(allocateResource(requestedRes, pid) == 1) { // Check if we are good to request some resources
					reqGranted++; // Up the request granted
					if(reqGranted % 20 == 0) { // Every 20 we want to print the logtable
						logTable = 1; 
					}
					strcpy(msgbuf.msg,"GRANTED"); // Write that it was granted
					msgbuf.mtype = pid;
					msgsnd(toChild,&msgbuf,sizeof(msgbuf),IPC_NOWAIT);
					if(verbose == true) { // Print this if verbose is true
						logOutput(logfile, "Master: Process P%d granted resource R%d at time %d:%d\n",pid,requestedRes,shared->time.seconds,shared->time.nanoseconds);
					}
				} else { // If we are not ready
					if(verbose == true) { // Print this if verbose is true
						logOutput(logfile, "Master: Process P%d moved to waiting queue for resource R%d at time %d:%d\n",pid,requestedRes,shared->time.seconds,shared->time.nanoseconds);
					}
					enqueue(waiting,pid); // Put it into queue
				}
			}
		}
		int k = 0;
		if(empty(waiting) == 0) { // Check if we are waiting
			int size = queueSize(waiting); // Grab the size of the queue
			while(k < size) { // While the size of the queue is greater than 0
				int pid = dequeue(waiting);
				int requestedRes;
				
				int i;
				for(i = 0; i < 20; i++) { // Check the resources
					if(shared->resources[i].request[pid - 1] > 0){
						requestedRes = i;
					}
				}

				if(allocateResource(requestedRes, pid) == 1) { // Check if we can allocate resources
					reqGranted++;
					if(reqGranted % 20 == 0) {
						logTable = 1; // Check if we should write to log table
					}
					if(verbose == true) { // Print if verbose is on
						logOutput(logfile, "Master: Resource R%d granted to process %d in wait status at time %d:%d\n",requestedRes,pid,shared->time.seconds,shared->time.nanoseconds);
					}
					
					strcpy(msgbuf.msg,"GRANTED"); // Write that we have granted resources
					msgbuf.mtype = pid;
					msgsnd(toChild,&msgbuf,sizeof(msgbuf),IPC_NOWAIT);
				} else {
					enqueue(waiting,pid); // Put into a queue
				}
				k++;
			}
		}

		if(shared->time.seconds == deadlockClock.seconds) { // Check if seconds are the same
			deadlockClock.seconds++;
			int k;
			int deadlock;
			if(empty(waiting) == 0) { // If nothing is wait we are in deadlock
				deadlock = 1; 
				printf("Deadlock detected at %d:%d\n",shared->time.seconds,shared->time.nanoseconds); // Print that a deadlock is found
				logOutput(logfile, "Deadlock detected at %d:%d\n",shared->time.seconds,shared->time.nanoseconds);
			} else { // Otherwise there is no deadlock
				deadlock = 0;
			}
			if(deadlock == 1) {
				while(1) { // We are to kill pid's if the process is deadlocked, deal with the deadlocked problem
					deadlock = 0;
					int pidToKill = dequeue(waiting);
					msgbuf.mtype = pidToKill;
					strcpy(msgbuf.msg,"TERM"); // Set it to terminate
					msgbuf.mtype = pidToKill;
					msgsnd(toChild,&msgbuf,sizeof(msgbuf),IPC_NOWAIT);
					while(waitpid(pids[pidToKill - 1],NULL, 0) > 0);
					proTerminated++; // Add to the number of terminated
					printf("Process P%d terminated by deadlock algorithm at time %d:%d\n",pidToKill,shared->time.seconds,shared->time.nanoseconds);
					logOutput(logfile, "===> Process P%d terminated by deadlock algorithm at time %d:%d\n",pidToKill,shared->time.seconds,shared->time.nanoseconds);
					
					int m;
					if(verbose == true) { // Write if verbose is on
						logOutput(logfile, "Resources being released are:\n");
					}
					
					for(m = 0; m < 20; m++) { // Check through our potential processes
						if(shared->resources[m].shareable == 0) { // Check if it is shareable
							if(shared->resources[m].allocated[pidToKill - 1] > 0) {
								shared->resources[m].available += shared->resources[m].allocated[pidToKill - 1];
								if(verbose == 1) { // Write if verbose is on
									logOutput(logfile, "R%d ",m);
								}

							}
						}
						shared->resources[m].allocated[pidToKill - 1] = 0; // Reset whether it is allocated or requesting
						shared->resources[m].request[pidToKill - 1] = 0;
					}
					logOutput(logfile, "\n");
					if(verbose == true) {
						logOutput(logfile, "\n");
					}
					activeChild--; // Subtract from the amount of children out right now
					slots[pidToKill - 1] = 0;
					count++;
					k = 0;
					while(k < queueSize(waiting)) { // Check if anything is still in queue
						int temp = dequeue(waiting);
						releaseResource(temp,verbose); // Release the resource
						k++;
					}

					if(empty(waiting) != 0) {
						break;
					}
				}
				if(verbose == true) { // Log if verbose is on
					logOutput(logfile, "No longer in deadlock\n");
				}
			}
		}

		if((reqGranted % 20 == 0 && reqGranted != 0) && logTable == 1 && verbose == true) { // If it is time to write the table and if verbose is on
			i = 0; // Reset our counters for our table
			k = 0;
			logOutput(logfile, "\n");
			logOutput(logfile, "------------------------------------------------------------------------------------------------------------------------------------------------------\n");
			logOutput(logfile, "|                                                             Allocation Table                                                                        |\n");
			logOutput(logfile, "------------------------------------------------------------------------------------------------------------------------------------------------------\n");
			for(i = 0; i < 18; i++) {
				if(slots[i] == 1) {
				logOutput(logfile, "P:%d  ",i + 1); // Print our slots
					if(i <= 8) {
					logOutput(logfile, "| %2s%d  |","P",i+1);

					}
					for(k = 0; k < 20; k++) { // Print whether or not the resources are currently allocated
						logOutput(logfile, "  %2d  |", shared->resources[k].allocated[i]);
					}
					logOutput(logfile, "\n");
				}
			}
			logOutput(logfile, "------------------------------------------------------------------------------------------------------------------------------------------------------\n");
			logOutput(logfile, "\n");
			logTable = 0;
		}
	} while((proTerminated + proExited) < 40); // Want to stay in this loop until we are done
	exitSignal(0);	
}

void releaseResource(int pid , bool verbose) { // Releasing our resources
	int r_id, i; 
	for(i = 0; i < 20; i++) { // Setting the resource id's 
		if(shared->resources[i].request[pid - 1] > 0) {
			r_id = i;
		}
	}

	if(allocateResource(r_id, pid) == 1) {
		if(verbose == true) { // Check if we have verbose logging on
			logOutput(logfile, "Master has detected resource R%d given to process P%d at time %d:%d\n",r_id,pid,shared->time.seconds,shared->time.nanoseconds);
		}
		reqGranted++; // Add to the list of requests granted
		if(reqGranted % 20 == 0) { // Figure out if we should print the table
			logTable = 1;
		}
		strcpy(msgbuf.msg,"GRANTED"); // Write that this has been granted
		msgbuf.mtype = pid;
		msgsnd(toChild,&msgbuf,sizeof(msgbuf),IPC_NOWAIT);
	} else { // Put it into a wait queue
		enqueue(waiting,pid);
	}
}

void deallocateResource(int resID, int pid) { // Deallocate of resource
	if(shared->resources[resID].shareable == 0) { // Check to make sure it is not shareable
		shared->resources[resID].available += shared->resources[resID].allocated[pid - 1];
	}

	shared->resources[resID].allocated[pid - 1] = 0;
}

int allocateResource(int resID, int pid) { // Allocation of our resources
	while((shared->resources[resID].request[pid - 1] > 0 && shared->resources[resID].available > 0)) { // Make sure it is avaliable
		if(shared->resources[resID].shareable == 0) { // Check whether or not it is shareable
			(shared->resources[resID].request[pid - 1])--; // Update how many are avaliable now
			(shared->resources[resID].allocated[pid - 1])++;
			(shared->resources[resID].available)--;
		} else { // In case it is shareable
			shared->resources[resID].request[pid - 1] = 0;
			break;
		}
	}
	if(shared->resources[resID].request[pid - 1] > 0) { // Want to make sure we can request anymore
		return -1;
	} else {
		return 1;
	}
}

int freeID() { // Freeing the IDs
	int i;
	for(i = 0; i < 18; i++) { // Go through all our possible pids
		if(slots[i] == 0) {
			slots[i] = 1;
			return i;
		}
	}
	return -1;
}

void addClock(struct time* time, int seconds, int nanoseconds) {
	time->seconds += seconds;
	time->nanoseconds += nanoseconds;
	while(time->nanoseconds >= 1000000000) { // Updating of our clock and making sure seconds have passed if at all
		time->nanoseconds -=1000000000;
		time->seconds++;
	}
}

void exitSignal(int sig) { // If one of our signals happens
	switch(sig) {
		case SIGALRM: // Print in the case of an alarm
			printf("\nSimulation finished at end of timer.\n");
			break;
		case SIGINT: // Print in case of a control c
			printf("\nSimulation finished on control+c.\n");
			break;
		default: 
			break;
	}
	// Print our summary and totals of what happened during our run
	logOutput(logfile, "\n|                                             Summary                                                            |\n");
	logOutput(logfile, "------------------------------------------------------------------------------------------------------------------\n");


	logOutput(logfile, "Total Requests Granted: %d\n", reqGranted);
	logOutput(logfile, "Total processes terminated from deadlock: %d\n", proTerminated);
	logOutput(logfile, "Total processes terminated normally: %d\n", proExited);

	printf("\nSummary\n");
	printf("Total Requests Granted: %d\n", reqGranted);
	printf("Total processes terminated from deadlock: %d\n", proTerminated);
	printf("Total processes terminated normally: %d\n", proExited);

	cleanResources(); // Clean everything 
	kill(0,SIGKILL); // Kill our process
}

void cleanResources() { // Clean our messages and our shared memory
	msgctl(ossQueue,IPC_RMID,NULL);
	msgctl(toChild,IPC_RMID,NULL);
	shmdt((void*)shared);
	shmctl(shmID, IPC_RMID, NULL);
}
