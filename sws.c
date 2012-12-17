#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <readline/readline.h>
#include <readline/history.h>


#include "ccsignal.h"
#include "ccsocket.h"
#include "ccresponse.h"
#include "daemonize.h"

#define MAX_LEN 128
#define REQUEST_LINE_MAX 1024

int c_flag = 0;
int d_flag = 0;
int h_flag = 0;
int i_flag = 0;
int l_flag = 0;
char *dir, *CGI_dir, *log_file, *addr = NULL;
int port = 8080;
int logfd;
char loginfo[1024];

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

	if(h_flag == 1){
		usage();
		exit(0);
	}

	if(d_flag == 0)
		daemonize("sws");	

	if(l_flag == 1){
		if(d_flag == 1)
			printf("[INFO]Checking the validation of log file specified after -l ...\n");		
	
		char real_logfile[MAX_LEN] = "";
		if(realpath(log_file,real_logfile) == NULL){
			if(d_flag == 1)
				fprintf(stderr,"[ERROR]The given log file is not in a good path\n");
			else
				syslog(LOG_ERR,"[ERROR]The given log file is not in a good path\n");
			exit(1);	
		}

		if((logfd = open(real_logfile,O_RDWR | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0){
			if(d_flag == 1)
				fprintf(stderr,"[ERROR]Open log file error, cannot start server\n");
			else
				syslog(LOG_ERR,"[ERROR]Open log file error, cannot start server\n");
			exit(1);
		}
		
		if(d_flag == 1)
			printf("[INFO]Checking successfully\n");
	}	

	if(signal(SIGCHLD,sig_cld) < 0){
		if(d_flag == 1)
			perror("[ERROR]signal handler set error\n");
		else
			syslog(LOG_ERR,"[ERROR]signal handler set error\n");
		exit(1);
	}

	if(d_flag == 1)
		printf("[INFO]Checking the validation of root directory of server you specified ...\n");
	
	dir = argv[optind];	

	if(dir == NULL){
		if(d_flag == 1){
			fprintf(stderr,"[ERROR]Missing dir arguments. Fail to start server.\nUsage:\n");
			usage();
		}
		else
			syslog(LOG_ERR,"[ERROR]Missing dir arguments. Fail to start server.\n");
		exit(1);
	}

	char server_root[MAX_LEN];
	if(realpath(dir,server_root) == NULL){
		if(d_flag == 1){
			fprintf(stderr,"[ERROR]Server root directory error\nCheck the validation of the root directory\n");
			perror(strerror(errno));
		}
		else
			syslog(LOG_ERR,"[ERROR]Server root directory error\nCheck the validation of the root directory\n");
		exit(1);	
	}

	struct stat root_buf;
	if(stat(server_root,&root_buf) == -1){
		if(d_flag == 1)
			fprintf(stderr,"[ERROR]Stat server root error\n");	
		else
			syslog(LOG_ERR,"[ERROR]Stat server root error\n");
		exit(1);
	}

	if(!S_ISDIR(root_buf.st_mode)){
		if(d_flag == 1)
			fprintf(stderr,"[ERROR]Root must be a directory, please check the directory you specified\n");
		else
			syslog(LOG_ERR,"[ERROR]Root must be a directory, please check the directory you specified\n");
		exit(1);
	}

	if(d_flag == 1)
		printf("[INFO]Checking finished -- Success\n");

	char cgi_root[MAX_LEN];
	if(c_flag == 1){
		if(d_flag == 1)
			printf("[INFO]Checking the validation of CGI root you specified after -c ...\n");

		if(CGI_dir == NULL){
			if(d_flag == 1){
				fprintf(stderr,"[ERROR]Missing cgi root directory\n");
				usage();
			}
			else
				syslog(LOG_ERR,"[ERROR]Missing cgi root directory\n");
			exit(1);
		}

		if(realpath(CGI_dir,cgi_root) == NULL){
			if(d_flag == 1){
				fprintf(stderr,"[ERROR]cgi root directory error\nCheck the validation of the directory\n");
				perror(strerror(errno));
			}
			else
				syslog(LOG_ERR,"[ERROR]cgi root directory error\nCheck the validation of the directory\n");
			exit(1);	
		}
		
		struct stat cgi_buf;
		if(stat(cgi_root,&cgi_buf) == -1){
			if(d_flag == 1)
				fprintf(stderr,"[ERROR]Stat cgi root error, cannot start server\n");
			else
				syslog(LOG_ERR,"[ERROR]Stat cgi root error, cannot start server\n");
			exit(1);
		}

		if(!S_ISDIR(cgi_buf.st_mode)){
			if(d_flag == 1)
				fprintf(stderr,"[ERROR]The CGI root you specified must be a directory\n");
			else
				syslog(LOG_ERR,"[ERROR]The CGI root you specified must be a directory\n");
			exit(1);
		}
		//printf("cgi_root, %s \n", cgi_root);
		if(d_flag == 1)
			printf("[INFO]Checking finished -- Success\n");
	}

	/*
	* Creating the socket
	*/
	int sockfd;
	if((sockfd = init_listen(i_flag,addr,port)) < 0){
		if(d_flag == 1)
			fprintf(stderr,"[ERROR]cannot start server\n");
		else
			syslog(LOG_ERR,"[ERROR]cannot start server\n");
		exit(1);
	}

	int client_fd;
	while(1){	
		/*
		* Check the ip address specified after -i
		* If not specified, use IPv6 to handle both IPv4 and IPv6
		* If specified, check what type is the IP address after -i
		*/
		char incomming[INET6_ADDRSTRLEN];
		struct addrinfo hint,*result;
		memset(&hint,0,sizeof(struct addrinfo));
		result = (struct addrinfo *)malloc(sizeof(struct addrinfo));
		hint.ai_family = AF_UNSPEC;
		/*
		* specified, check type
		*/
		if(addr != NULL)
			getaddrinfo(addr,0,&hint,&result);
		
		/*
		* Not specified, use IPv6 by default.
		*/
		if(addr == NULL)
			result -> ai_family = AF_INET6;

		/*
		* If it is a IPv6, then using IPv6 address family to create,bind,listen.
		*/
		if(result -> ai_family == AF_INET6){
			struct sockaddr_in6 remote;
			socklen_t receive = INET6_ADDRSTRLEN;
			memset(&remote, 0, sizeof(struct sockaddr_in6));

			if((client_fd = accept(sockfd,(struct sockaddr *)&remote,&receive)) < 0){
				if(d_flag == 1){
					fprintf(stderr,"[ERROR]accept error\n");
					perror(strerror(errno));
				}
				else
					syslog(LOG_ERR,"[ERROR]accept error\n");
				exit(1);
			}	
			/*
			* Get the incomming request address
			*/
			if(inet_ntop(AF_INET6,&remote.sin6_addr.s6_addr,incomming,INET6_ADDRSTRLEN) == NULL){
				perror(strerror(errno));
			}
		}
		
		/*
		* If it is a IPv4, using IPv4 address family.
		*/
		if(result -> ai_family == AF_INET){
			struct sockaddr_in remote;
			socklen_t receive = INET_ADDRSTRLEN;
			memset(&remote, 0, sizeof(struct sockaddr_in));

			if((client_fd = accept(sockfd,(struct sockaddr *)&remote,&receive)) < 0){
				if(d_flag == 1){
					fprintf(stderr,"[ERROR]accept error\n");
					perror(strerror(errno));
				}
				else
					syslog(LOG_ERR,"[ERROR]accept error\n");
				exit(1);
			}	
			/*
			* Get the incomming request address
			*/
			if(inet_ntop(AF_INET,&remote.sin_addr.s_addr,incomming,INET_ADDRSTRLEN) == NULL){
				perror(strerror(errno));
			}
		}

		if(d_flag == 1)
			printf("[INFO]One request from IP address: %s\n",incomming);
	
		/*
		* Record the logging infomation into loginfo buffer
		*/
		strcpy(loginfo,"[INFO]One request comes from IP address: ");
		strcat(loginfo,incomming);
		strcat(loginfo,"\n");
		strcat(loginfo,"[INFO]The request conclusion:\n");

		/*
		* Logging the request comming time
		*/
		time_t t = time(NULL);
		strcat(loginfo,"[INFO]Request time: ");
		strcat(loginfo,ctime(&t));

		int cid;

		if((cid = fork()) == 0){	/*child process, handle the coming request*/
			int receive_b = 0, i = 0;
			char totalheader[2*REQUEST_LINE_MAX] = "";
			char request_line[REQUEST_LINE_MAX] = "";
			char check_method[MAX_LEN] = "";
			int datasize = 0, datareading = 0, datagot = 0;
			/*
			* Read the request line.
			* Waiting until the request line is not just \n\r.	
			*/
			while(1){	
				receive_b = read(client_fd,totalheader + strlen(totalheader),REQUEST_LINE_MAX);

				/*
				* This if condition is used to control the data reading for POST request
				* If the request is from browser: totalheader will contain the post data after the CRLF(\r\n\r\n)
				* If the request is from telnet command line: Must waiting for the user input the data after the CRLF
				*/
				if(datareading == 1){
					datagot += receive_b;
					/*
					* The read data is less than the length specified in the header
					*/
					if(datagot < datasize)
						continue;
					/*
					* Read enough data, break;
					*/
					else
						break;
				}
				
				/*
				* This is used to waiting for the user give some actual input instead of just pressing enter.
				*/
				if(receive_b == 2 && strlen(totalheader) == 2){
					totalheader[0] = 0;
					continue;
				}
				
				/*
				* If there is \r\n\r\n existing in the total header.
				* This means that if the method is a GET or HEAD request
				* The request header has finished. We can start to process the request
				* If it is a POST method, this means that the following will be the data sent to the requested file.
				* Then we check the Content-Length to get the data length.
				* If no content-length in the header, then 411 missing content length give back.
				* If has the content-length but no data after \r\n\r\n. We read until get content-length much data.
				* If has data, we pull the data out into a buffer.
				*/
				if(strstr(totalheader,"\r\n\r\n") != NULL){	
					/*
					* Check the methdo type. Only support GET POST HEAD these three method
					*/
					while(i < 4){
						check_method[i] = totalheader[i];
						if(i  == 2){
							if(strcmp(check_method,"GET") != 0){
								i++;
								continue;
							}
							else
								break;
						}
						if(i == 3){
							/*
							* The request method is not GET POST or HEAD
							* 501 not implemented error send back
							*/
							if(strcmp(check_method,"POST") != 0 && strcmp(check_method,"HEAD") != 0){
								header(client_fd,501,root_buf,0);
								exit(0);
							}
							else
								break;
						}	
						i++;
					}
					
					if(strcmp(check_method,"POST") == 0){
						char *lengthbegin;
						char tempbuf[MAX_LEN] = "";
						/*
						* No content-length, break;
						*/
						if((lengthbegin = strcasestr(totalheader,"Content-Length")) == NULL){ //If the request header has no content length
							header(client_fd,411,root_buf,0);
							exit(0);	
						}
						/*
						* Has content-length, then get the length
						*/
						else{
							memset(tempbuf,0,strlen(tempbuf));
							i = 0;
							lengthbegin += 15;
							while(*lengthbegin != 13){
								tempbuf[i] = *lengthbegin;
								lengthbegin ++;
								i++;
							}
							/*
							* This is the content length
							*/
							datasize = atoi(tempbuf); 

							/*
							* If the \r\n\r\n is not at the end of the total header
							* This means there are Datas after the empty line.
							* We don't need to read the data any more. Just break;
							*/
							if(totalheader[strlen(totalheader) - 3] != '\n')
								break;
							
							/*
							* If the \r\n\r\n is at the end of the total header. 
							* This means we need to reading the POST data from the socket fd
							* Continue to read the data until read no less than datasize. Then we break;
							*/								
							else{ 
								datareading = 1;
								continue;
							}
						}
					}
					else
						break;
				}
			}
			
			/*
			* Fetch the data which will be used by the POST request from the totalheader
			*/
			char postdata[datasize];
			i = 0;
			char *startpoint = NULL;

			/*
			* Find the starting point of the postdata.
			*/
			startpoint = strstr(totalheader,"\r\n\r\n");
			startpoint += 4;
			
			/*
			* Fetch the data into postdata buffer
			*/
			while(i < datasize){
				postdata[i] = *startpoint;
				i ++;
				startpoint++;
			}		
	
			/*
			* Fetch the request line from the totalheader
			*/
			i = 0;
			while(totalheader[i] != 13){
				request_line[i] = totalheader[i];
				i++;
			}
			
			/*
			* Recording the request line info in order to log the info
			*/	
			strcat(loginfo,"[INFO]Request line: ");
			strcat(loginfo,request_line);
			strcat(loginfo,"\n");

			/*
			* Analyse the request line. parse it into three part
			* method, path and protocol
			*/
			char method[MAX_LEN] = "", path[MAX_LEN] = "", protocol[MAX_LEN] = ""; 
			sscanf(request_line,"%s %s %s",method,path,protocol);

			/*
			* Make the . operator work
			*/
			if(strcmp(path,".") == 0)
				path[0] = '/';

			char cgi_check[9] = "";
			for(i = 0; i < 9; i++)
				cgi_check[i] = path[i];

			if(c_flag == 1 && strcmp(cgi_check,"/cgi-bin/") == 0){
				if(d_flag == 1)
					printf("[INFO]CGI request ... Begin processing ... \n");
				cgi_response(client_fd,method,path,protocol,cgi_root,postdata);
			}
			else{
				if(d_flag == 1)
					printf("[INFO]Normal request ... Begin processing ...\n");
				response(client_fd,method,path,protocol,server_root,postdata);
			}	
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


