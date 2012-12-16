#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>

#define MAX_CONNECTION 10
extern int d_flag;

int init_listen(int i_flag, char *addr, int port);

int init_listen(int i_flag, char *addr, int port){
	int sockfd;
	
	/*
	* If i flag is not set, then use IPv6 to handle both IPv4 and IPv6 request
	*/
	if(i_flag == 0){
		if(d_flag == 1)
			printf("[INFO]Creating Socket ...\n");

		/*
		* Create the socket, IPv6
		*/
		if((sockfd = socket(AF_INET6,SOCK_STREAM,0)) < 0){
			if(d_flag == 1)
				fprintf(stderr,"[ERROR]generate socket error\n");
			else
				syslog(LOG_ERR,"[ERROR]generate socket error\n");
			exit(1);
		}
		if(d_flag == 1)
			printf("[INFO]Creating Socket successfully ...\n");

		struct sockaddr_in6 name;
		name.sin6_family = AF_INET6;
		name.sin6_port = htons(port);
		name.sin6_addr = in6addr_any;

		if(d_flag == 1){
			printf("[INFO]Listening on all IPv4 and IPv6 addresses\n");
			printf("[INFO]Binding sockets ..\n");
		}

		/*
		* Binding the socket to the port number(default is 8080)
		*/
		if(bind(sockfd,(struct sockaddr *)&name, sizeof(name))){
			if(d_flag == 1){
				fprintf(stderr,"[ERROR]bind error,cannot bind socket to the given IP ADDRESS or port is already in use\n");
				perror(strerror(errno));
			}
			else
				syslog(LOG_ERR,"[ERROR]bind error,cannot bind socket to the given IP ADDRESS or port is already in use\n");
			exit(1);
		}

		if(d_flag == 1)
			printf("[INFO]Binding successfully\n");
	}

	/*
	* If i flag is set, then check whether it is a IPv4 or IPv6 address
	*/
	if(i_flag == 1){
		struct addrinfo hint,*result = NULL;
		memset(&hint,0,sizeof(struct addrinfo));
		hint.ai_family = AF_UNSPEC;
		getaddrinfo(addr,0,&hint,&result);
		
		/*
		* It is a IPv4 address, using IPv4 address family
		*/
		if(result -> ai_family == AF_INET){	
			if(d_flag == 1)
				printf("[INFO]Creating Socket ...\n");
	
			/*
			* Create the socket, IPv4
			*/
			if((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0){
				if(d_flag == 1)
					fprintf(stderr,"[ERROR]generate socket error\n");
				else
					syslog(LOG_ERR,"[ERROR]generate socket error\n");
				exit(1);
			}
			if(d_flag == 1)
				printf("[INFO]Creating Socket successfully ...\n");

			struct sockaddr_in name;
			name.sin_family = AF_INET;
			name.sin_port = htons(port);
			inet_pton(AF_INET,addr,&name.sin_addr.s_addr);

			if(d_flag == 1){
				printf("[INFO]Listening on IPv4 address: %s\n",addr);
				printf("[INFO]Binding sockets ...\n");
			}

			/*
			* Binding the socket
			*/
			if(bind(sockfd,(struct sockaddr *)&name, sizeof(name))){
				if(d_flag == 1){
					fprintf(stderr,"[ERROR]bind error,cannot bind socket to the given IP ADDRESS or port is already in use\n");
					perror(strerror(errno));
				}
				else
					syslog(LOG_ERR,"[ERROR]bind error,cannot bind socket to the given IP ADDRESS or port is already in use\n");
				exit(1);
			}

			if(d_flag == 1)
				printf("[INFO]Binding successfully\n");
		}

		/*
		* If it is a IPv6 address, using IPv6 address family
		*/
		if(result -> ai_family == AF_INET6){
			if(d_flag == 1)
				printf("[INFO]Creating Socket ...\n");
	
			/*
			* Create the socket IPv6
			*/
			if((sockfd = socket(AF_INET6,SOCK_STREAM,0)) < 0){
				if(d_flag == 1)
					fprintf(stderr,"[ERROR]generate socket error\n");
				else
					syslog(LOG_ERR,"[ERROR]generate socket error\n");
				exit(1);
			}
			if(d_flag == 1)
				printf("[INFO]Creating Socket successfully ...\n");

			struct sockaddr_in6 name;
			name.sin6_family = AF_INET6;
			name.sin6_port = htons(port);
			inet_pton(AF_INET6,addr,&name.sin6_addr);

			if(d_flag == 1){
				printf("[INFO]Listening on IPv6 address: %s\n",addr);
				printf("[INFO]Binding sockets ...\n");
			}

			/*
			* Binding the socket
			*/
			if(bind(sockfd,(struct sockaddr *)&name, sizeof(name))){
				if(d_flag == 1){
					fprintf(stderr,"[ERROR]bind error,cannot bind socket to the given IP ADDRESS or port is already in use\n");
					perror(strerror(errno));
				}
				else
					syslog(LOG_ERR,"[ERROR]bind error,cannot bind socket to the given IP ADDRESS or port is already in use\n");
				exit(1);
			}

			if(d_flag == 1)
				printf("[INFO]Binding successfully\n");
		}
	}
	
	/*
	* Listening on the socket
	*/
	if(listen(sockfd,MAX_CONNECTION) == -1){
		if(d_flag == 1)
			fprintf(stderr,"[ERROR]listen error\n");
		else
			syslog(LOG_ERR,"[ERROR]listen error\n");
		exit(1);
	}
		
	if(d_flag == 1)
		printf("[INFO]Waiting for request comming ... \n");

	return sockfd;
}
