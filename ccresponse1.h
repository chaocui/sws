#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <syslog.h>

#ifndef MAX_LEN
#define MAX_LEN 128
#endif

#define NOT_FOUND 2
#define BAD_REQUEST 1
#define DIR_GOOD 3
#define FILE_MAKEGOOD 5
#define FILE_GOOD 6 
#define SERVER_ERROR -1000
#define OUT_ROOT 2
#define NO_DIR 1
#define HEAD 0
#define GET 1

void responsetime(int client_fd);
void serverinfo(int client_fd);
int respcode(char *method, char *path, char *protocol, char *cwd);
int checkdir(char *dir, char *cwd);
char* makedir(char *path);
char* getfilename(char *path);
void resplastmodifytypelength(int client_fd, int result, char *path, char *cwd);
int respmessage(int check, int client_fd, char *message, char *path, char *cwd);
void senddata(int fd, int client_fd);
void sendwrap(int fd, int client_fd,int senddataflag,struct stat stat_buf);
void processfile(char *file, int client_fd, int dataflag);
void emptyheader(int client_fd);
void cgi_response(int client_fd,char *method,char *path, char *protocol,char *cgi_root);
void response(int client_fd, char *method, char *path, char *protocol, char *cwd){

void responsetime(int client_fd){
	time_t t;
	char *buf;
	t = time(NULL);
	buf = ctime(&t);
	write(client_fd,"Date: ",6);
	write(client_fd,buf,30);
	syslog(LOG_CRIT,"jahahahahahha");	
}

void serverinfo(int client_fd){
	char info[] = "SERVER INFOMATION: simple web server, version 1.0\n";
	write(client_fd,info,sizeof(info));	
}

int respcode(char *method, char *path, char *protocol, char *cwd){
	char fullpath[MAX_LEN];
	strcpy(fullpath,cwd);
	strcat(fullpath,path);
	struct stat stat_buf;
	if(stat(fullpath,&stat_buf) == -1){
		return NOT_FOUND;
	}
	
	if(S_ISDIR(stat_buf.st_mode))
		strcat(fullpath,"/");	

	if(strcmp(method,"GET") != 0 && strcmp(method,"HEAD") != 0 && strcmp(method,"POST") && 0){ //400 bad request
		return BAD_REQUEST;
	} 

	//else if(strcmp(protocol,"HTTP/1.0") != 0){ // 400 bad request
	//	return BAD_REQUEST;
	//}

	else if(path[2] != '~'){

		if(fullpath[strlen(fullpath) -1] == '/'){ //request is a directory
			int check = checkdir(fullpath,cwd);

			if(check == NO_DIR){ // 404 not found
				return NOT_FOUND;
			}

			else if(check == OUT_ROOT){//has such dir, but outside the server root, give not found feed back. 404 not found
				return BAD_REQUEST;
			}

			else{// request OK, good request, need to further check whether has the permission
				return DIR_GOOD;
			}
		}
		
		else{  					//request is a file
			char temp[MAX_LEN], *tempa;
			tempa = makedir(fullpath);
			strcpy(temp,tempa);
			int check = checkdir(temp,cwd);

			if(checkdir(temp,cwd) == NO_DIR){//no such file, 404 not found
				return NOT_FOUND;
			}
			else if(check == OUT_ROOT){//has such dir, but outside the server root, so automaticly change the dir to root. 404 not found

				char *file;
				file = getfilename(fullpath);
				char cwdcpy[MAX_LEN];
				strcpy(cwdcpy,cwd);
				strcat(cwdcpy,"/");
				strcat(cwdcpy,file);
				printf("file name %s\n", cwdcpy);

				if(access(cwd,F_OK) == 0){ //has such file in the root, good request, further check needed
					return FILE_MAKEGOOD;
				}

				else{     //no such file in the root, 404 not found
					return NOT_FOUND;
				}

			}
			else{ //has such dir, the dir is not out of the root directory. So request is valid, need to check whether has the file requested. 
				if(access(fullpath,F_OK) == 0){ //has such file in the root, further check needed
					return FILE_GOOD;
				}

				else{     //no such file in the root, 404 not found
					return NOT_FOUND;
				}
			}
		}	
	}
	
	else if(path[2] == '~'){
		
	}
	return 0;
}

int respmessage(int response, int client_fd, char *method, char *path, char *cwd){
	int senddataflag;
	if(strcmp(method,"HEAD") == 0)
		senddataflag = HEAD;
	if(strcmp(method,"GET") == 0)
		senddataflag = GET;
	write(client_fd,"HTTP ",5);
	
	if(response == BAD_REQUEST)
		write(client_fd,"400 bad request\n",16);
	
	if(response == NOT_FOUND)
		write(client_fd, "404 not found\n",14);
	
	char fullpath[MAX_LEN];
	strcpy(fullpath,cwd);
	strcat(fullpath,path);
	
	if(response == DIR_GOOD){
		if(access(fullpath,R_OK) == 0){
			strcat(fullpath,"/");
			strcat(fullpath,"index.html");
			if(access(fullpath,R_OK) == 0){//has index.html in the directory 
				struct stat stat_buf;
				int tempfd;
				if(stat(fullpath,&stat_buf) == -1)
					tempfd = SERVER_ERROR;
				if((tempfd = open(fullpath, O_RDONLY)) <  0)
					tempfd = SERVER_ERROR;
	
				if(tempfd == SERVER_ERROR){ //server error
					write(client_fd,"500 server inner error\n",23);
					emptyheader(client_fd);
				}
				else{
					write(client_fd, "200 request OK\n", 15);
					sendwrap(tempfd,client_fd,senddataflag,stat_buf);
				}
			}
			else{ //does not have the index.html, retrieve the directory.
				char *processdir = makedir(fullpath);
				char dir_inuse[MAX_LEN];
				strcpy(dir_inuse,processdir);
				DIR *dp;
				struct stat stat_buf;

				if(stat(dir_inuse,&stat_buf) == -1){
					write(client_fd,"500 server inner error stat\n",27);
					emptyheader(client_fd);

				}

				if((dp = opendir(dir_inuse)) == NULL){
					write(client_fd,"500 server inner error open\n",27);
					emptyheader(client_fd);
				}

				else{
					struct dirent *dir_buf;
					write(client_fd,"200 request OK\n",15);
					responsetime(client_fd);
					serverinfo(client_fd);
					char buf[100];
					int j = sprintf(buf,"Last Modification time : %s",ctime(&stat_buf.st_mtime));		
					write(client_fd,buf,j);
					write(client_fd,"Content type: test/html\n\n",25);

					while((dir_buf = readdir(dp)) != NULL){
						write(client_fd,dir_buf -> d_name, strlen(dir_buf -> d_name));
						write(client_fd,"\n",1);
					}
				}	
			}
		}
		else{
			write(client_fd, "403 access denied\n",18);
			emptyheader(client_fd);
		}
	}
	
	if(response == FILE_GOOD)
		processfile(fullpath,client_fd,senddataflag);
		
	
	if(response == FILE_MAKEGOOD){
		char *filename;
		filename = getfilename(path);
		strcat(cwd,"/");
		strcat(cwd,filename);
		printf("FILE_MAKEGOOD %s\nfilename %s\n",cwd,filename);
		processfile(cwd,client_fd,senddataflag);
	}

	if(response == BAD_REQUEST || response == NOT_FOUND)
		emptyheader(client_fd);

	return 0;	
}

void emptyheader(int client_fd){
	responsetime(client_fd);
	serverinfo(client_fd);
	write(client_fd,"Last modification time: N/A\n",28);
	write(client_fd,"content type: N/A\n",18);	
	write(client_fd,"content length: N/A\n",20);
	write(client_fd,"\n",1);
}

void processfile(char *file, int client_fd, int dataflag){
	if(access(file,R_OK) == 0){//has access to file
		struct stat stat_buf;
		int tempfd;
		if(stat(file,&stat_buf) == -1)
			tempfd = SERVER_ERROR;
		if((tempfd = open(file, O_RDONLY)) <  0)
			tempfd = SERVER_ERROR;

		if(tempfd == SERVER_ERROR){ //server error
			write(client_fd,"500 server inner error\n",23);
			emptyheader(client_fd);
		}
		else{
			write(client_fd, "200 request OK\n", 15);
			sendwrap(tempfd,client_fd,dataflag,stat_buf);
		}	
	}
	else{ //no access to file
		write(client_fd, "403 access denied\n",18);
		emptyheader(client_fd);
	}
}

void sendwrap(int fd,int client_fd,int senddataflag,struct stat stat_buf){
	char buf[300];
	int j = sprintf(buf,"Last Modification time : %s",ctime(&stat_buf.st_mtime));		
	j += sprintf(buf+j,"Content type: test/html\n");
	j += sprintf(buf+j,"Content length: %d\n\n",(int)stat_buf.st_size);
	responsetime(client_fd);
	serverinfo(client_fd);
	write(client_fd,buf,j);
	if(senddataflag == GET)
		senddata(fd,client_fd);
	
}

void response(int client_fd, char *method, char *path, char *protocol, char *cwd){
	int response = respcode(method,path,protocol,cwd);	
	respmessage(response,client_fd,method,path,cwd);
}

int checkdir(char *dir, char *cwd){
	if(chdir(dir) == -1){
		return NO_DIR;	
	}
	else{
		char cwd_p[MAX_LEN];
		getcwd(cwd_p,sizeof(cwd_p));
		int length1 = strlen(cwd);
		int length2 = strlen(cwd_p);
		if(length2 < length1) //trying to request a file outside the root directory of the server
			return OUT_ROOT;
		if(length2 > length1){
			cwd_p[length1] = 0;
			if(strcmp(cwd_p,cwd) != 0)//trying to request a file outside the root directory of the server
				return OUT_ROOT;
			else
				return 0;
		}	
	}	
	return 0;
}

char *makedir(char *path){
	char dest[MAX_LEN];
	strcpy(dest,path);
	int length = strlen(dest);
	while(dest[length-1] != '/')
		length--;
	dest[length] = 0;
	char *result = dest;
	return result;	
}

char *getfilename(char *path){
	char dest[MAX_LEN];
	strcpy(dest,path);
	int length = strlen(dest);
	while(dest[length-1] != '/')
		length--;
	char file[MAX_LEN];
	int j = 0, i = length;
	for(j = 0; dest[i] != 0; i++){
		file[j] = dest[i];
		j++;
	}
	file[j] = 0;
	char *result;
	result = file;
	return result;		
}

void senddata(int fd, int sockdes){
	char temp[1024];
	int readbyte;
	while((readbyte = read(fd,temp,1024)) > 0){
		if(write(sockdes,temp,readbyte) != readbyte){
			fprintf(stderr,"write error\n");
			exit(1);
		}
	}
	write(sockdes,"\n",1);
}

