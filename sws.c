#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "ccsocket.h"

int c_flag = 0;
int d_flag = 0;
int h_flag = 0;
int i_flag = 0;
int l_flag = 0;
int p_flag = 0;
char *dir, *CGI_dir, *log_file, *addr;
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
				p_flag = 1;
				port = atoi(optarg);
				break;
		}
	}	
	dir = argv[optind];	
	if(dir == NULL){
		fprintf(stderr,"Missing dir arguments. Fail to start daemon process.\nUsage:\n");
		usage();
		exit(1);
	}
	int sockfd;
	if((sockfd = init_listen(1,NULL,port)) < 0){
		fprintf(stderr,"cannot start server\n");
		exit(1);
	}

	char buf[1024];

	if(read(sockfd,buf,1024) < 0){
		fprintf("stderr", "Reading from socket error");
		exit(1);
	}
	
	printf("%s",buf);	

	return 0;
}

void usage(){
	printf("sws [options:-c:dhi:l:p:] dir\n");
}
