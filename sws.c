#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "ccsignal.h"
#include "ccsocket.h"
#include "ccresponse.h"
#include "daemonize.h"

#define MAX_LEN 128

int c_flag = 0;
int d_flag = 0;
int h_flag = 0;
int i_flag = 0;
int l_flag = 0;
char *dir, *CGI_dir, *log_file, *addr = NULL;
int port = 8080;

void usage();

int main(int argc, char **argv){
	char opt;
	while((opt = getopt(argc,argv,"c:dhi:l:p:")) != -1){
		switch(opt){
			case 'c':
				c_flag = 1;
				CGI_dir = optarg;
				break;
			case 'd':
				d_flag = 1;
				break;
			case 'h':
				h_flag = 1;
				break;
			case 'i':
				i_flag = 1;
				addr = optarg;
				break;
			case 'l':
				l_flag = 1;
				log_file = optarg;
				break;
			case 'p':
				port = atoi(optarg);
				break;
		}
	}
	
	if(d_flag == 0)
		daemonize("sws");	

	if(signal(SIGCHLD,sig_cld) < 0){
		perror("signal handler set error\n");
		exit(1);
	}

	dir = argv[optind];	
	if(dir == NULL){
		fprintf(stderr,"Missing dir arguments. Fail to start daemon process.\nUsage:\n");
		usage();
		exit(1);
	}

	if(chdir(dir) == -1){
		fprintf(stderr,"Server root directory error\nCheck the validation of the root directory\n");
		exit(1);	
	}
	
	int sockfd;
	if((sockfd = init_listen(i_flag,addr,port)) < 0){
		fprintf(stderr,"cannot start server\n");
		exit(1);
	}

	struct sockaddr remote;
	int client_fd;
	socklen_t receive;
	while(1){	
		if((client_fd = accept(sockfd,(struct sockaddr*)&remote,&receive)) < 0){
			fprintf(stderr,"accept error");
			exit(1);
		}	
		int cid;
		char buf[1024];
		if((cid = fork()) == 0){	/*child process, handle the coming request*/
			char cwd[MAX_LEN];
			getcwd(cwd,sizeof(cwd));
			int receive_b;
			receive_b = read(client_fd,buf,1024);
			buf[receive_b] = '\0';
			char method[MAX_LEN] = "", path[MAX_LEN] = "", protocol[MAX_LEN] = ""; 
			sscanf(buf,"%s %s %s",method,path,protocol);
			response(client_fd,method,path,protocol,cwd);
	
			exit(0);
		}
		else{ 			/*parent process, waiting for new request*/
			close(client_fd);
		}	
	}
	return 0;
}

void usage(){
	printf("sws [options:-c:dhi:l:p:] dir\n");
}


