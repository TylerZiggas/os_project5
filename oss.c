#include "oss.h"

FILE *fp;
#define log_file "osslog.txt"
const int max_log_lines = 1000000;
int line_count = 0;
int toChild;
int oss_q;
int shmid;
int slots[18];

struct{
	long mtype;
	char msg[10];
}msgbuf;

struct sharedRes *shared;
struct Queue *waiting;


int request_granted = 0;
int procs_terminated = 0;
int procs_exited = 0;
int log_table = 1;

int main(int argc, char *argv[]){
	int verbose = 0, c;
	while((c=getopt(argc, argv, "v:h"))!= EOF){
		switch(c){
			case 'h':
				printf("-v: 1== Verbose on, 0 == Verbose off.\n");
				exit(0);
				break;
			case 'v':
				verbose = atoi(optarg);
				if(verbose == 1){
					printf("Verbose is on\n");
				}else if(verbose == 0){
					printf("Verbose is off\n");
				}
				if(verbose > 1 || verbose < 0){
					printf("Invalid option. Setting v to 0.  -h for more details\n");
					verbose = 0;
				}
				break;
		}
	}

	srand(time(NULL));
	key_t key;
	key = ftok(".",'a');
	if((shmid = shmget(key,sizeof(struct sharedRes), IPC_CREAT | 0666)) == -1){
		perror("shmget");
		exit(1);
	}

	shared = (struct sharedRes*)shmat(shmid,(void*)0,0);
	if(shared == (void*)-1){
		perror("Error on attaching memory");
	}

	key_t msgkey;
	if((msgkey = ftok("Makefile",925)) == -1){
		perror("ftok");
	}

	if((toChild = msgget(msgkey, 0600 | IPC_CREAT)) == -1){
		perror("msgget");
	}

	if((msgkey = ftok("Makefile",825)) == -1){
		perror("ftok");
	}

	if((oss_q = msgget(msgkey, 0600 | IPC_CREAT)) == -1){
		perror("msgget");
	}

	fp = fopen(log_file,"w");
	int i, j, index;
	for(i = 0; i < 20; i++){
		shared->resources[i].shareable = 0;
		int temp = (rand() % (10 - 1 + 1)) + 1; 
		shared->resources[i].instances = temp;
		shared->resources[i].available = temp;
		for(j = 0; j <= 18; j++){
			shared->resources[i].request[j] = 0;
			shared->resources[i].release[j] = 0;
			shared->resources[i].allocated[j] = 0;
		}
	}
	int randShareable = (rand() % (5 - 1 + 1)) + 1;
	for(i = 0; i < randShareable; i++){
		
		index = (rand() % (19 - 0 + 1 )) + 0;
		shared->resources[index].shareable = 0;
	}

	printf("Starting OSS \n");
	printf("Log File ====> osslog.txt now...\n");

	signal(SIGALRM, oss_exit_signal);
	alarm(2);
	signal(SIGINT, oss_exit_signal);

	run_simulation(verbose);
	clean_resources(shmid, shared);
	exit(EXIT_SUCCESS);
}
void run_simulation(int verbose){

	int i = 0;
	struct time fork_clock;
	struct time deadlock_clock;
	deadlock_clock.seconds = 1;

	int nextFork = (rand() % (500000000 - 1000000 + 1)) + 1000000;
	add_clock(&fork_clock,0,nextFork);

	int active_child = 0;
	int count = 0;
	pid_t pids[18]; 
	waiting = createQueue(100);
	do{

		add_clock(&shared->time,0,10000);

		if((active_child < 18) && ((shared->time.seconds > fork_clock.seconds) || (shared->time.seconds == fork_clock.seconds && shared->time.nanoseconds >= fork_clock.nanoseconds))){

			
			fork_clock.seconds = shared->time.seconds;
			fork_clock.nanoseconds = shared->time.nanoseconds;
			nextFork = (rand() % (500000000 - 1000000 + 1)) + 1000000;
			add_clock(&fork_clock,0,nextFork);
			int newProc = get_free_slot_id() + 1;
			if((newProc-1 > -1)){
				pids[newProc - 1] = fork();
				if(pids[newProc - 1] == 0){
					char str[10];
					sprintf(str, "%d", newProc);
					execl("./user_proc",str,NULL);
					exit(0);
				}
				active_child++;
				if(line_count < max_log_lines){
					fprintf(fp,"Master spwaned Process P%d; time %d:%d\n",newProc,shared->time.seconds,shared->time.nanoseconds);
					line_count++;
				}
			}
		}

		if(msgrcv(oss_q, &msgbuf, sizeof(msgbuf),0,IPC_NOWAIT) > -1){
			int pid = msgbuf.mtype;
			if (strcmp(msgbuf.msg, "TERMINATED") == 0){
				while(waitpid(pids[pid - 1],NULL, 0) > 0);
				int m;
				procs_exited++;
				for(m = 0; m < 20; m++){
					if(shared->resources[m].shareable == 0){
						if(shared->resources[m].allocated[pid -1] > 0){
							shared->resources[m].available+=
							shared->resources[m].allocated[pid - 1];
						}
					}
					shared->resources[m].allocated[pid - 1] = 0;
					shared->resources[m].request[pid - 1] = 0;
				}
				active_child--;
				slots[pid - 1] = 0;
				count++;
				if(line_count < max_log_lines){
					fprintf(fp,"Master: Process P%d terminated; time %d:%d\n",pid,shared->time.seconds,shared->time.nanoseconds);
					line_count++;
				}

			}
			else if (strcmp(msgbuf.msg, "RELEASE") == 0){
				msgrcv(oss_q, &msgbuf,sizeof(msgbuf),pid,0);
				int releasedRes = atoi(msgbuf.msg);
				if(releasedRes > -1){
					deallocateResource(releasedRes,pid);
					if(line_count < max_log_lines && verbose == 1){
						fprintf(fp,"Master: Process P%d released resource R%d; time %d:%d\n",pid,releasedRes,shared->time.seconds,shared->time.nanoseconds);
						line_count++;
					}
				}
			}
			else if (strcmp(msgbuf.msg, "REQUEST") == 0){
				msgrcv(oss_q, &msgbuf, sizeof(msgbuf),pid,0);
				int requestedRes = atoi(msgbuf.msg);

				if(line_count < max_log_lines && verbose == 1){
					fprintf(fp,"Master: Process P%d requesting R%d; time %d:%d\n",pid,requestedRes,shared->time.seconds,shared->time.nanoseconds);
					line_count++;
				}

				msgrcv(oss_q, &msgbuf, sizeof(msgbuf),pid,0);
				int instances = atoi(msgbuf.msg);

				shared->resources[requestedRes].request[pid - 1] = instances;

				if(allocate_resource(requestedRes, pid) == 1){
					request_granted++;
					if(request_granted % 20 == 0){
						log_table = 1;
					}
					strcpy(msgbuf.msg,"GRANTED");
					msgbuf.mtype = pid;
					msgsnd(toChild,&msgbuf,sizeof(msgbuf),IPC_NOWAIT);
					if(line_count < max_log_lines && verbose ==1){
						fprintf(fp,"Master: Process P%d granted resource R%d; time %d:%d\n",pid,requestedRes,shared->time.seconds,shared->time.nanoseconds);
						line_count++;
					}
				}else{
					if(line_count < max_log_lines && verbose == 1){
						fprintf(fp,"Master: Process P%d moved to  waiting queue for resource R%d; time %d:%d\n",pid,requestedRes,shared->time.seconds,shared->time.nanoseconds);
						line_count++;
					}
					enqueue(waiting,pid);
				}
			}
		}
		int k = 0;
		if(empty(waiting) == 0){
			int size = queueSize(waiting);
			while(k < size){
				int pid = dequeue(waiting);
				int requestedRes;

				
				int i;
				for(i = 0; i < 20; i++){
					if(shared->resources[i].request[pid - 1] > 0){
						requestedRes = i;
					}
				}

				if(allocate_resource(requestedRes, pid) == 1){
					request_granted++;
					if(request_granted % 20 == 0){
						log_table = 1;
					}
					if(line_count < max_log_lines && verbose == 1){
						line_count++;
						fprintf(fp,"Master: Resource R%d granted to process %d  in wait status; time %d:%d\n",requestedRes,pid,shared->time.seconds,shared->time.nanoseconds);
					}
					
					strcpy(msgbuf.msg,"GRANTED");
					msgbuf.mtype = pid;
					msgsnd(toChild,&msgbuf,sizeof(msgbuf),IPC_NOWAIT);
				}else{
					
					enqueue(waiting,pid);
				}
				k++;
			}
		}

		if(shared->time.seconds == deadlock_clock.seconds){
			deadlock_clock.seconds++;
			int k;
			int deadlock;
			if(empty(waiting) == 0){
				deadlock = 1;
				if(line_count < max_log_lines){
					line_count++;
					fprintf(fp,"Mater has detected deadlock at %d:%d\n",shared->time.seconds,shared->time.nanoseconds);
				}
			}else{
				deadlock = 0;
			}
			if(deadlock == 1){
				while(1){
					deadlock = 0;
					int pidToKill = dequeue(waiting);
					msgbuf.mtype = pidToKill;
					strcpy(msgbuf.msg,"TERM");
					msgbuf.mtype = pidToKill;
					msgsnd(toChild,&msgbuf,sizeof(msgbuf),IPC_NOWAIT);
					while(waitpid(pids[pidToKill - 1],NULL, 0) > 0);
					procs_terminated++;

					if(line_count < max_log_lines){
						line_count++;
						fprintf(fp,"===> Process P%d  terminated by deadlock algoritm; time %d:%d\n",pidToKill,shared->time.seconds,shared->time.nanoseconds);
					}
					int m;
					if(line_count < max_log_lines && verbose == 1){
						line_count++;
						fprintf(fp,"Resources released are:\n");
						fprintf(fp,"\n");
					}
					
					for(m = 0; m < 20; m++){
						if(shared->resources[m].shareable == 0){
							if(shared->resources[m].allocated[pidToKill - 1] > 0){
								shared->resources[m].available+=
								shared->resources[m].allocated[pidToKill - 1];
								if(line_count < max_log_lines && verbose == 1){
									line_count++;
									fprintf(fp,"R%d ",m);
								}

							}
						}
						shared->resources[m].allocated[pidToKill - 1] = 0;
						shared->resources[m].request[pidToKill - 1] = 0;
					}
					if(verbose == 1){
						fprintf(fp,"\n");
					}
					active_child--;
					slots[pidToKill - 1] = 0;
					count++;
					k = 0;
					while(k < queueSize(waiting)){
						int temp = dequeue(waiting);
						try_release_resource(temp,verbose);
						k++;
					}

					if(empty(waiting) != 0){
						break;
					}
				}
				if(line_count < max_log_lines && verbose == 1){
					fprintf(fp,"Mater is no longer in deadlock\n");
				}
			}
		}

		if((request_granted % 20 == 0 && request_granted != 0) && log_table == 1 && verbose == 1){
			i = 0;
			k = 0;
			fprintf(fp,"\n");
			fprintf(fp,"----------------------------------------------------------------------------------------------------------------------------------------------------\n");
			fprintf(fp,"|                                                            Allocation Table                                                                       |\n");
			fprintf(fp,"----------------------------------------------------------------------------------------------------------------------------------------------------\n");
			for(i = 0; i < 18; i++){
				if(slots[i] == 1){
				fprintf(fp,"P:%d  ",i + 1);
					if(i <= 8){
					fprintf(fp,"| %2s%d  |","P",i+1);

					}
					for(k = 0; k < 20; k++){
						fprintf(fp,"  %2d  |", shared->resources[k].allocated[i]);

					}
					fprintf(fp,"\n");
				}
			}
			fprintf(fp,"----------------------------------------------------------------------------------------------------------------------------------------------------\n");

			fprintf(fp,"\n");
			line_count+=18;
			log_table = 0;
		}
	}while(1) ;



}

void try_release_resource(int pid,int verbose){
	int r_id, i; 
	for(i = 0; i < 20; i++){
		if(shared->resources[i].request[pid - 1] > 0){
				r_id = i;
			}
		}

		if(allocate_resource(r_id, pid) == 1){
			if(line_count < max_log_lines && verbose == 1){
				line_count++;
				fprintf(fp,"Mater has detected resource R%d given to process P%d at time %d:%d\n",r_id,pid,shared->time.seconds,shared->time.nanoseconds);
			}
			request_granted++;
			if(request_granted % 20 == 0){
				log_table = 1;
			}
			strcpy(msgbuf.msg,"GRANTED");
			msgbuf.mtype = pid;
			msgsnd(toChild,&msgbuf,sizeof(msgbuf),IPC_NOWAIT);
		}else{
			enqueue(waiting,pid);
		}
}

void deallocateResource(int resource_id, int pid){
	if(shared->resources[resource_id].shareable == 0){
		shared->resources[resource_id].available += shared->resources[resource_id].allocated[pid - 1];
	}

	shared->resources[resource_id].allocated[pid - 1] = 0;
}

int allocate_resource(int resource_id, int pid){
	while((shared->resources[resource_id].request[pid - 1] > 0 &&
		shared->resources[resource_id].available > 0)){
		if(shared->resources[resource_id].shareable == 0){
			(shared->resources[resource_id].request[pid - 1])--;
			(shared->resources[resource_id].allocated[pid - 1])++;
			(shared->resources[resource_id].available)--;
		}else{
			shared->resources[resource_id].request[pid - 1] = 0;
			break;
		}
	}
	if(shared->resources[resource_id].request[pid - 1] > 0){
		return -1;
	}else{
		return 1;
	}
}

int get_free_slot_id(){
	int i;
	for(i = 0; i < 18; i++){
		if(slots[i] == 0){
			slots[i] = 1;
			return i;
		}
	}
	return -1;
}

void add_clock(struct time* time, int sec, int ns){
	time->seconds += sec;
	time->nanoseconds += ns;
	while(time->nanoseconds >= 1000000000){
		time->nanoseconds -=1000000000;
		time->seconds++;
	}
}

void oss_exit_signal(int sig){
	switch(sig){
		case SIGALRM:
			printf("\nSimulation finised (2 seconds). The program will now terminate.\n");
			break;
		case SIGINT:
			printf("\nctrl+c has been registered. Now exiting.\n");
			break;
	}
	fprintf(fp,"----------------------------------------------------------------------------------------------------------------------------------------------------\n");
	fprintf(fp,"|                                                            Summary                                                                                |\n");
	fprintf(fp,"----------------------------------------------------------------------------------------------------------------------------------------------------\n");


	fprintf(fp,"Total Requests Granted: %d\n",request_granted);
	fprintf(fp,"Total processes terminated from deadlock: %d\n", procs_terminated);
	fprintf(fp,"Total processes terminated normally: %d\n", procs_exited);

	printf("Total Requests Granted: %d\n",request_granted);
	printf("Total processes terminated from deadlock: %d\n", procs_terminated);
	printf("Total processes terminated normally: %d\n", procs_exited);

	clean_resources();
	kill(0,SIGKILL);
}

void clean_resources(){
	fclose(fp);
	msgctl(oss_q,IPC_RMID,NULL);
	msgctl(toChild,IPC_RMID,NULL);
	shmdt((void*)shared);
	shmctl(shmid, IPC_RMID, NULL);
}
