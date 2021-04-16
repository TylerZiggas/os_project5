#include "oss.h"

void add_time(struct time* time, int sec, int ns){
	time->seconds += sec;
	time->nanoseconds += ns;
	while(time->nanoseconds >= 1000000000){
		time->nanoseconds -=1000000000;
		time->seconds++;
	}
}


struct{
	long mtype;
	char msg[10];
}msgbuf;
struct sharedRes *shared;
int shmid;
int toChild;
int oss_q;



const int RELEASE_OR_REQUEST = 1000000;
const int REQUEST_RESOURCE = 70;
const int DIE = 10;


int main(int argc, char *argv[]){
	time_t t;
	time(&t);	
	srand((int)time(&t) % getpid());
	

	key_t key;
	key = ftok(".",'a');
	if((shmid = shmget(key,sizeof(struct sharedRes),0666)) == -1){
		perror("shmget");
		exit(0);	
	}
	
	shared = (struct sharedRes*)shmat(shmid,(void*)0,0);
	if(shared == (void*)-1){
		perror("Error on attaching memory");
		exit(1);
	}
	

	key_t msgkey;
	if((msgkey = ftok("Makefile",925)) == -1){
		perror("ftok");
		exit(1);
	}

	if((toChild = msgget(msgkey, 0600 | IPC_CREAT)) == -1){
		perror("msgget");
		exit(1);
	}	
	
	if((msgkey = ftok("Makefile",825)) == -1){
		perror("ftok");
		exit(1);
	}

	if((oss_q = msgget(msgkey, 0600 | IPC_CREAT)) == -1){
		perror("msgget");	
		exit(1);
	}
	int pid = atoi(argv[0]);
	

	int interval = (rand() % RELEASE_OR_REQUEST + 1);
	struct time action_clock;
	action_clock.seconds = shared->time.seconds;
	action_clock.nanoseconds = shared->time.nanoseconds;
	add_time(&action_clock, 0, interval);

	
	int termination = (rand() % (250 * 1000000) + 1);
	struct time should_terminate;
	should_terminate.seconds = shared->time.seconds;
	should_terminate.nanoseconds = shared->time.nanoseconds;
	add_time(&should_terminate, 0, termination);

	do{	

		if((shared->time.seconds > action_clock.seconds) || (shared->time.seconds == action_clock.seconds && shared->time.nanoseconds >= action_clock.nanoseconds)){

			action_clock.seconds = shared->time.seconds;
			action_clock.nanoseconds = shared->time.nanoseconds;
			add_time(&action_clock, 0, interval);	
	
			if((rand() % 100) < REQUEST_RESOURCE){	
				strcpy(msgbuf.msg,"REQUEST");
				msgbuf.mtype = pid;
				msgsnd(oss_q,&msgbuf,sizeof(msgbuf),0);
	

				int resourceToRequest = (rand() %20);
				sprintf(msgbuf.msg, "%d", resourceToRequest);
				msgsnd(oss_q, &msgbuf,sizeof(msgbuf),0);
				

				int instances = (rand()% (shared->resources[resourceToRequest].instances)) + 1;
				
				sprintf(msgbuf.msg, "%d", instances);
				msgsnd(oss_q, &msgbuf,sizeof(msgbuf),0);
				while(1){
					msgrcv(toChild,&msgbuf,sizeof(msgbuf),pid,0);
					if(strcmp(msgbuf.msg, "GRANTED") == 0){
						break;
					}
					if(strcmp(msgbuf.msg, "TERM") == 0){
						exit(0);
					}
				}
			}else{
				strcpy(msgbuf.msg,"RELEASE");
				msgbuf.mtype = pid;
				msgsnd(oss_q,&msgbuf,sizeof(msgbuf),0);

				
				int resource = -1;
				int i;
				
				for(i = 0; i < 20; i++){
					if(shared->resources[i].allocated[pid - 1] > 0){
						resource = i;
						break ; 
					}	
				}
				sprintf(msgbuf.msg, "%d", resource);
				msgsnd(oss_q,&msgbuf, sizeof(msgbuf),0);
			}
		}

		if((shared->time.seconds > should_terminate.seconds) || (shared->time.seconds == should_terminate.seconds && shared->time.nanoseconds >= should_terminate.nanoseconds)){
			

			termination = (rand() % (250 * 1000000) + 1);
			should_terminate.seconds = shared->time.seconds;
			should_terminate.nanoseconds = shared->time.nanoseconds;
			add_time(&should_terminate, 0, termination);
			if((rand()%100) <= DIE){
				strcpy(msgbuf.msg,"TERMINATED");
				msgbuf.mtype = pid;
				msgsnd(oss_q,&msgbuf,sizeof(msgbuf),0);	
				exit(0);
			}
		}
	}while(1) ; 
	return 0 ; 
}
