#include "oss.h"

int main (int argc, char* argv[]) {
	int timerSec = 5;
	char* logfile = "logfile";
	createFile(logfile);
	pid_t parentID = getpid();
	sigact(SIGINT, signalHandler);
	setupTimer(timerSec);
	logOutput(logfile, "This is a test %s", getFormattedTime());
	
	sleep(100);
	
	return 0;
}
