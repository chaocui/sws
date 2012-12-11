#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define MAX_CONNECTION 10

int init_listen(int i_flag, char *addr, int port);

int init_listen(int i_flag, char *addr, int port){
	int sockfd;
	struct sockaddr_in name;
	if((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0){
		fprintf(stderr,"generate socket error\n");
		exit(1);
	}
	
	name.sin_family = AF_INET;
	name.sin_port = htons(port);
	if(i_flag == 0)
		name.sin_addr.s_addr = INADDR_ANY;
	if(i_flag == 1)
		inet_pton(AF_INET,addr,&name.sin_addr);
	
	if(bind(sockfd,(struct sockaddr *)&name, sizeof(name))){
		fprintf(stderr,"bind error,cannot bind socket to the given IP ADDRESS\nUse a valid IP within this machine\n");
		perror(strerror(errno));
		exit(1);
	}

	if(listen(sockfd,MAX_CONNECTION) == -1){
		fprintf(stderr,"listen error\n");
		exit(1);
	}

	return sockfd;
}
