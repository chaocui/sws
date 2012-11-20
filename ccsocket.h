#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>

struct sockaddr_in name;

int init_listen(int all, in_addr_t addr, int port);

int create_connection();

int init_listen(int all, in_addr_t addr, int port){
	int sockfd;
	if((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0){
		fprintf(stderr,"generate socket error\n");
		exit(1);
	}
	
	name.sin_family = AF_INET;
	name.sin_port = htons(port);
	if(all == 1)
		name.sin_addr.s_addr = INADDR_ANY;
	if(all == 0)
		name.sin_addr.s_addr = addr;
	
	if(bind(sockfd,(struct sockaddr *)&name, sizeof(name))){
		fprintf(stderr,"bind error");
		exit(1);
	}
	return sockfd;
}
