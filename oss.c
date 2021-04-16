#include "oss.h"

int toChild;
int oss_q;
int shmid;
int slots[18];

struct{
	long mtype;
	char msg[10];
} msgbuf;

struct sharedRes *shared;
struct Queue *waiting;

int request_granted = 0;
int procs_terminated = 0;
int procs_exited = 0;
int log_table = 1;
char* logfile = "logfile";

int main(int argc, char *argv[]) {
	int character;
	bool verbose = false;
	createFile(logfile);
	while((character = getopt(argc, argv, "vh"))!= EOF) {
		switch(character) {
			case 'v':
				verbose = true;
				printf("Verbose is on\n");
				break;
			case 'h':
				printf("This is invoked by ./oss and you may add -v after to turn on verbose logging.\n");
				exit(EXIT_SUCCESS);
			default:
				printf("That is not a valid parameter.\n");
				exit(EXIT_FAILURE);
		}
	}

	srand(time(NULL));
	key_t key;
	key = ftok(".",'a');
	if((shmid = shmget(key,sizeof(struct sharedRes), IPC_CREAT | 0666)) == -1) {
		perror("shmget");
		exit(1);
	}

	shared = (struct sharedRes*)shmat(shmid,(void*)0,0);
	if(shared == (void*)-1) {
		perror("Error attaching memory");
	}

	key_t msgkey;
	if((msgkey = ftok("Makefile",925)) == -1) { 
		perror("ftok");
	}

	if((toChild = msgget(msgkey, 0600 | IPC_CREAT)) == -1) {
		perror("msgget");
	}

	if((msgkey = ftok("Makefile",825)) == -1) {
		perror("ftok");
	}

	if((oss_q = msgget(msgkey, 0600 | IPC_CREAT)) == -1) {
		perror("msgget");
	}

	int i, j, index;
	for(i = 0; i < 20; i++) {
		shared->resources[i].shareable = 0;
		int temp = (rand() % (10 - 1 + 1)) + 1; 
		shared->resources[i].instances = temp;
		shared->resources[i].available = temp;
		for(j = 0; j <= 18; j++) {
			shared->resources[i].request[j] = 0;
			shared->resources[i].release[j] = 0;
			shared->resources[i].allocated[j] = 0;
		}
	}
	int randShareable = (rand() % (5 - 1 + 1)) + 1;
	for(i = 0; i < randShareable; i++) { 
		index = (rand() % (19 - 0 + 1 )) + 0;
		shared->resources[index].shareable = 0;
	}

	printf("Starting OSS simulation...\n");

	signal(SIGALRM, exitSignal);
	alarm(5);
	signal(SIGINT, exitSignal);

	simulateOSS(verbose);
	cleanResources(shmid, shared);
	exit(EXIT_SUCCESS);
}
void simulateOSS(bool verbose) {

	int i = 0;
	struct time fork_clock;
	struct time deadlock_clock;
	deadlock_clock.seconds = 1;

	int nextFork = (rand() % (500000000 - 1000000 + 1)) + 1000000;
	addClock(&fork_clock,0,nextFork);

	int active_child = 0;
	int count = 0;
	pid_t pids[18]; 
	waiting = createQueue(100);
	do {
		addClock(&shared->time,0,10000);
		if((active_child < 18) && ((shared->time.seconds > fork_clock.seconds) || (shared->time.seconds == fork_clock.seconds && shared->time.nanoseconds >= fork_clock.nanoseconds))) {
			fork_clock.seconds = shared->time.seconds;
			fork_clock.nanoseconds = shared->time.nanoseconds;
			nextFork = (rand() % (500000000 - 1000000 + 1)) + 1000000;
			addClock(&fork_clock,0,nextFork);
			int newProc = freeID() + 1;
			if((newProc-1 > -1)) {
				pids[newProc - 1] = fork();
				if(pids[newProc - 1] == 0){
					char str[10];
					sprintf(str, "%d", newProc);
					execl("./user_proc", str, NULL);
					exit(0);
				}
				active_child++;
				logOutput(logfile,"Master spawned Process P%d at time %d:%d\n",newProc,shared->time.seconds,shared->time.nanoseconds);
			}
		}

		if(msgrcv(oss_q, &msgbuf, sizeof(msgbuf),0,IPC_NOWAIT) > -1) {
			int pid = msgbuf.mtype;
			if (strcmp(msgbuf.msg, "TERMINATED") == 0) {
				while(waitpid(pids[pid - 1],NULL, 0) > 0);
				int m;
				procs_exited++;
				for(m = 0; m < 20; m++) {
					if(shared->resources[m].shareable == 0) {
						if(shared->resources[m].allocated[pid -1] > 0) {
							shared->resources[m].available += shared->resources[m].allocated[pid - 1];
						}
					}
					shared->resources[m].allocated[pid - 1] = 0;
					shared->resources[m].request[pid - 1] = 0;
				}
				active_child--;
				slots[pid - 1] = 0;
				count++;
				logOutput(logfile, "Master: Process P%d terminated at time %d:%d\n",pid,shared->time.seconds,shared->time.nanoseconds);

			} else if (strcmp(msgbuf.msg, "RELEASE") == 0) {
				msgrcv(oss_q, &msgbuf,sizeof(msgbuf),pid,0);
				int releasedRes = atoi(msgbuf.msg);
				if(releasedRes > -1) {
					deallocateResource(releasedRes,pid);
					if(verbose == true) {
						logOutput(logfile,"Master: Process P%d released resource R%d at time %d:%d\n",pid,releasedRes,shared->time.seconds,shared->time.nanoseconds);
					}
				}
			} else if (strcmp(msgbuf.msg, "REQUEST") == 0) {
				msgrcv(oss_q, &msgbuf, sizeof(msgbuf),pid,0);
				int requestedRes = atoi(msgbuf.msg);

				if(verbose == true) {
					logOutput(logfile, "Master: Process P%d requesting R%d at time %d:%d\n",pid,requestedRes,shared->time.seconds,shared->time.nanoseconds);
				}

				msgrcv(oss_q, &msgbuf, sizeof(msgbuf),pid,0);
				int instances = atoi(msgbuf.msg);

				shared->resources[requestedRes].request[pid - 1] = instances;

				if(allocateResource(requestedRes, pid) == 1) {
					request_granted++;
					if(request_granted % 20 == 0){
						log_table = 1;
					}
					strcpy(msgbuf.msg,"GRANTED");
					msgbuf.mtype = pid;
					msgsnd(toChild,&msgbuf,sizeof(msgbuf),IPC_NOWAIT);
					if(verbose == true) {
						logOutput(logfile, "Master: Process P%d granted resource R%d at time %d:%d\n",pid,requestedRes,shared->time.seconds,shared->time.nanoseconds);
					}
				} else {
					if(verbose == true) {
						logOutput(logfile, "Master: Process P%d moved to waiting queue for resource R%d at time %d:%d\n",pid,requestedRes,shared->time.seconds,shared->time.nanoseconds);
					}
					enqueue(waiting,pid);
				}
			}
		}
		int k = 0;
		if(empty(waiting) == 0) {
			int size = queueSize(waiting);
			while(k < size) {
				int pid = dequeue(waiting);
				int requestedRes;

				
				int i;
				for(i = 0; i < 20; i++) {
					if(shared->resources[i].request[pid - 1] > 0){
						requestedRes = i;
					}
				}

				if(allocateResource(requestedRes, pid) == 1) {
					request_granted++;
					if(request_granted % 20 == 0) {
						log_table = 1;
					}
					if(verbose == true) {
						logOutput(logfile, "Master: Resource R%d granted to process %d in wait status at time %d:%d\n",requestedRes,pid,shared->time.seconds,shared->time.nanoseconds);
					}
					
					strcpy(msgbuf.msg,"GRANTED");
					msgbuf.mtype = pid;
					msgsnd(toChild,&msgbuf,sizeof(msgbuf),IPC_NOWAIT);
				} else {
					
					enqueue(waiting,pid);
				}
				k++;
			}
		}

		if(shared->time.seconds == deadlock_clock.seconds) {
			deadlock_clock.seconds++;
			int k;
			int deadlock;
			if(empty(waiting) == 0) {
				deadlock = 1;
				logOutput(logfile, "Deadlock detected at %d:%d\n",shared->time.seconds,shared->time.nanoseconds);
			} else {
				deadlock = 0;
			}
			if(deadlock == 1) {
				while(1) {
					deadlock = 0;
					int pidToKill = dequeue(waiting);
					msgbuf.mtype = pidToKill;
					strcpy(msgbuf.msg,"TERM");
					msgbuf.mtype = pidToKill;
					msgsnd(toChild,&msgbuf,sizeof(msgbuf),IPC_NOWAIT);
					while(waitpid(pids[pidToKill - 1],NULL, 0) > 0);
					procs_terminated++;
					logOutput(logfile, "===> Process P%d terminated by deadlock algorithm at time %d:%d\n",pidToKill,shared->time.seconds,shared->time.nanoseconds);
					
					int m;
					if(verbose == true) {
						logOutput(logfile, "Resources being released are:\n\n");
					}
					
					for(m = 0; m < 20; m++){
						if(shared->resources[m].shareable == 0) {
							if(shared->resources[m].allocated[pidToKill - 1] > 0) {
								shared->resources[m].available+=
								shared->resources[m].allocated[pidToKill - 1];
								if(verbose == 1) {
									logOutput(logfile, "R%d ",m);
								}

							}
						}
						shared->resources[m].allocated[pidToKill - 1] = 0;
						shared->resources[m].request[pidToKill - 1] = 0;
					}
					if(verbose == true) {
						logOutput(logfile, "\n");
					}
					active_child--;
					slots[pidToKill - 1] = 0;
					count++;
					k = 0;
					while(k < queueSize(waiting)){
						int temp = dequeue(waiting);
						releaseResource(temp,verbose);
						k++;
					}

					if(empty(waiting) != 0) {
						break;
					}
				}
				if(verbose == true) {
					logOutput(logfile, "No longer in deadlock\n");
				}
			}
		}

		if((request_granted % 20 == 0 && request_granted != 0) && log_table == 1 && verbose == 1) {
			i = 0;
			k = 0;
			logOutput(logfile, "\n");
			logOutput(logfile, "----------------------------------------------------------------------------------------------------------------------------------------------------\n");
			logOutput(logfile, "|                                                            Allocation Table                                                                       |\n");
			logOutput(logfile, "----------------------------------------------------------------------------------------------------------------------------------------------------\n");
			for(i = 0; i < 18; i++) {
				if(slots[i] == 1) {
				logOutput(logfile, "P:%d  ",i + 1);
					if(i <= 8){
					logOutput(logfile, "| %2s%d  |","P",i+1);

					}
					for(k = 0; k < 20; k++) {
						logOutput(logfile, "  %2d  |", shared->resources[k].allocated[i]);

					}
					logOutput(logfile, "\n");
				}
			}
			logOutput(logfile, "----------------------------------------------------------------------------------------------------------------------------------------------------\n");

			logOutput(logfile, "\n");
			log_table = 0;
		}
	} while(1);
}

void releaseResource(int pid , bool verbose) {
	int r_id, i; 
	for(i = 0; i < 20; i++) {
		if(shared->resources[i].request[pid - 1] > 0) {
				r_id = i;
			}
		}

		if(allocateResource(r_id, pid) == 1) {
			if(verbose == true){
				logOutput(logfile, "Master has detected resource R%d given to process P%d at time %d:%d\n",r_id,pid,shared->time.seconds,shared->time.nanoseconds);
			}
			request_granted++;
			if(request_granted % 20 == 0) {
				log_table = 1;
			}
			strcpy(msgbuf.msg,"GRANTED");
			msgbuf.mtype = pid;
			msgsnd(toChild,&msgbuf,sizeof(msgbuf),IPC_NOWAIT);
		} else {
			enqueue(waiting,pid);
		}
}

void deallocateResource(int resource_id, int pid) {
	if(shared->resources[resource_id].shareable == 0) {
		shared->resources[resource_id].available += shared->resources[resource_id].allocated[pid - 1];
	}

	shared->resources[resource_id].allocated[pid - 1] = 0;
}

int allocateResource(int resource_id, int pid) {
	while((shared->resources[resource_id].request[pid - 1] > 0 && shared->resources[resource_id].available > 0)) {
		if(shared->resources[resource_id].shareable == 0){
			(shared->resources[resource_id].request[pid - 1])--;
			(shared->resources[resource_id].allocated[pid - 1])++;
			(shared->resources[resource_id].available)--;
		} else {
			shared->resources[resource_id].request[pid - 1] = 0;
			break;
		}
	}
	if(shared->resources[resource_id].request[pid - 1] > 0){
		return -1;
	} else {
		return 1;
	}
}

int freeID() {
	int i;
	for(i = 0; i < 18; i++) {
		if(slots[i] == 0) {
			slots[i] = 1;
			return i;
		}
	}
	return -1;
}

void addClock(struct time* time, int sec, int ns) {
	time->seconds += sec;
	time->nanoseconds += ns;
	while(time->nanoseconds >= 1000000000){
		time->nanoseconds -=1000000000;
		time->seconds++;
	}
}

void exitSignal(int sig) {
	switch(sig) {
		case SIGALRM:
			printf("\nSimulation finished at end of timer.\n");
			break;
		case SIGINT:
			printf("\nSimulation finished on control+c.\n");
			break;
	}
	logOutput(logfile, "----------------------------------------------------------------------------------------------------------------------------------------------------\n");
	logOutput(logfile, "|                                                            Summary                                                                                |\n");
	logOutput(logfile, "----------------------------------------------------------------------------------------------------------------------------------------------------\n");


	logOutput(logfile, "Total Requests Granted: %d\n",request_granted);
	logOutput(logfile, "Total processes terminated from deadlock: %d\n", procs_terminated);
	logOutput(logfile, "Total processes terminated normally: %d\n", procs_exited);

	printf("Total Requests Granted: %d\n",request_granted);
	printf("Total processes terminated from deadlock: %d\n", procs_terminated);
	printf("Total processes terminated normally: %d\n", procs_exited);

	cleanResources();
	kill(0,SIGKILL);
}

void cleanResources() {
	msgctl(oss_q,IPC_RMID,NULL);
	msgctl(toChild,IPC_RMID,NULL);
	shmdt((void*)shared);
	shmctl(shmid, IPC_RMID, NULL);
}
