#include <signal.h>
#include <sys/wait.h>

extern int d_flag;

static void sig_cld(int);

static void sig_cld(int signo){
	pid_t pid;
	int status;
	if(d_flag == 1)
		printf("[INFO]Request done\n\n");

	if(signal(SIGCHLD,sig_cld) ==  SIG_ERR){
		fprintf(stderr,"signal handler set error");
		exit(1);
	}
	
	if((pid = wait(&status)) < 0){
		perror("wait error");
		exit(1);
	}
}

