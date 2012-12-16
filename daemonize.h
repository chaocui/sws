#include <fcntl.h>
#include <sys/resource.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>

void daemonize(const char *cmd){

	int i, fd0, fd1, fd2;
	pid_t pid;
	struct rlimit rl;
	struct sigaction sa;

	//clear file creation mask
	umask(0);

	//get maximum number of file descriptors
	if(getrlimit(RLIMIT_NOFILE, &rl) < 0){
		fprintf(stderr,"%s cannot get file limit", cmd);
		exit(1);
	}
	
	//become a session leader to lose controlling TTY
	if((pid = fork()) < 0){
		fprintf(stderr,"%s cannot fork", cmd);
		exit(1);
	}
	
	else if(pid != 0) //parent
		exit(0);
	setsid();

	//ensure future opens won't allocate controlling TTYs
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if(sigaction(SIGHUP, &sa, NULL) < 0){
		fprintf(stderr,"%s cannot ignore SIGHUP", cmd);
		exit(1);
	}
	if((pid = fork()) < 0){
		fprintf(stderr,"%s cannot fork", cmd);
		exit(1);
	}
	else if(pid != 0)//parent
		exit(0);

	//change the current working directory to the root, so we won't
	//prevent file systems from being unmounted.
	/*
	if(chdir("/") < 0)
		err_quit("%s : cannot change directory to /");
	*/

	//close all open file descriptors
	if(rl.rlim_max == RLIM_INFINITY)
		rl.rlim_max = 1024;

	for(i = 0; i < rl.rlim_max; i++)
		close(i);
	
	//attach file descriptors 0,1, and 2 to /dev/null.
	fd0 = open("dev/null", O_RDWR);
	fd1 = dup(0);
	fd2 = dup(0);
	fd0 += 0;
	fd1 += 0;
	fd2 += 0;

	//initialize the log file
	
	openlog(cmd, LOG_CONS, LOG_DAEMON);
	/*if(fd0 != 0 || fd1 != 1 || fd2 != 2){
		syslog(LOG_ERR, "unexpected file descriptors %d %d %d",
		fd0,fd1,fd2);
		exit(1);
	}*/
}

