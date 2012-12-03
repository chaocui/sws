#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#ifndef MAX_LEN
#define MAX_LEN 128
#endif

#define NOT_FOUND 2
#define BAD_REQUEST 1
#define DIR_GOOD 3
#define FILE_MAKEGOOD 5
#define FILE_GOOD 6 

void responsetime(int client_fd);
void serverinfo(int client_fd);
int respcode(char *method, char *path, char *protocol, char *cwd);
int checkdir(char *dir, char *cwd);
char* makedir(char *path);
char* getfilename(char *path);
void resplastmodifytypelength(int client_fd, int result, char *path, char *cwd);
int respmessage(int check, int client_fd, char *message, char *path, char *cwd);
void senddata(char *fullpath, int client_fd);
void sendwrap(char *paht, int client_fd,int senddataflag);

void responsetime(int client_fd){
	time_t t;
	char *buf;
	t = time(NULL);
	buf = ctime(&t);
	write(client_fd,"Date: ",6);
	write(client_fd,buf,30);
}

void serverinfo(int client_fd){
	char info[] = "SERVER INFOMATION: This is the server run by Chao Cui, version 1.0\n";
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
	printf("%s\n%s\n",fullpath,cwd);

		if(fullpath[strlen(fullpath) -1] == '/'){ //request is a directory
			int check = checkdir(fullpath,cwd);

			if(check == 1){ // 404 not found
				return NOT_FOUND;
			}

			else if(check == 2){//has such dir, but outside the server root, give not found feed back. 404 not found
				return BAD_REQUEST;
			}

			else{// request OK, good request, need to further check whether has the permission
				return DIR_GOOD;
			}
		}
		
		else{  					//request is a file
			char *temp;
			temp = makedir(fullpath);
			int check = checkdir(temp,cwd);

			if(checkdir(temp,cwd) == 1){//no such file, 404 not found
				return NOT_FOUND;
			}
			else if(check == 2){//has such dir, but outside the server root, so automaticly change the dir to root. 404 not found

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
	return 0;
}

int respmessage(int response, int client_fd, char *method, char *path, char *cwd){
	int senddataflag = 0;
	if(strcmp(method,"GET") == 0)
		senddataflag = 1;
	write(client_fd,"HTTP ",5);	
	if(response == BAD_REQUEST)
		write(client_fd,"400 bad request\n",16);
	if(response == NOT_FOUND)
		write(client_fd, "404 not found\n",14);
	char fullpath[MAX_LEN];
	int canread = 1;
	strcpy(fullpath,cwd);
	strcat(fullpath,path);
	if(response == DIR_GOOD){
		if(access(fullpath,R_OK) == 0)
			write(client_fd, "200 request OK\n", 15);
		else{
			write(client_fd, "500 access denied\n",18);
			canread = 0;
		}
	}
	if(response == FILE_GOOD){
		if(access(fullpath,R_OK) == 0)
			write(client_fd, "200 request OK\n", 15);
		else{
			write(client_fd, "500 access denied\n",18);
			canread = 0;
		}
	}	
	if(response == FILE_MAKEGOOD){
		char *filename;
		filename = getfilename(path);
		strcat(cwd,"/");
		strcat(cwd,filename);
		printf("FILE_MAKEGOOD %s\nfilename %s\n",cwd,filename);
		if(access(cwd,R_OK) == 0)
			write(client_fd, "200 request OK\n", 15);
		else{
			write(client_fd, "500 access denied\n",18);
			canread = 0;
		}
	}

	
	responsetime(client_fd);
	serverinfo(client_fd);
	if(response == BAD_REQUEST || response == NOT_FOUND || canread == 0){
		write(client_fd,"Last modification time: N/A\n",28);
		write(client_fd,"content type: N/A\n",18);
		write(client_fd,"content length: N/A\n",20);
		write(client_fd,"\n",1);
	}		
	else{
		if(response == FILE_GOOD)
			sendwrap(fullpath,client_fd,senddataflag);	
		else if(response == DIR_GOOD){
			strcat(fullpath,"index.html");
			if(access(fullpath,R_OK) == 0)
				sendwrap(fullpath,client_fd,senddataflag);
			else{
					
			}	
		}		
		else if(response == FILE_MAKEGOOD)
			sendwrap(cwd,client_fd,senddataflag);	
	}	
	
	return 0;	
}

void sendwrap(char *path,int client_fd,int senddataflag){
	struct stat stat_buf;
	if(stat(path,&stat_buf) == -1){
		fprintf(stderr,"stat file error\n");
		exit(1);
	}
	char buf[300];
	int j = sprintf(buf,"Last Modification time : %s",ctime(&stat_buf.st_mtime));		
	j += sprintf(buf+j,"Content type: test/html\n");
	j += sprintf(buf+j,"Content length: %d\n\n",(int)stat_buf.st_size);
	write(client_fd,buf,j);
	if(senddataflag == 1)
		senddata(path,client_fd);
	
}

void response(int client_fd, char *method, char *path, char *protocol, char *cwd){
	int response = respcode(method,path,protocol,cwd);	
	respmessage(response,client_fd,method,path,cwd);
}

int checkdir(char *dir, char *cwd){
	printf("check dir: %s\n%s\n",dir,cwd);
	if(chdir(dir) == -1){
		return 1;	
	}
	else{
		char cwd_p[MAX_LEN];
		getcwd(cwd_p,sizeof(cwd_p));
		printf("after chdir: %s \n", cwd_p);
		int length1 = strlen(cwd);
		int length2 = strlen(cwd_p);
		if(length2 < length1) //trying to request a file outside the root directory of the server
			return 2;
		if(length2 > length1){
			cwd_p[length1] = 0;
			printf("%s\n",cwd_p);
			if(strcmp(cwd_p,cwd) != 0)//trying to request a file outside the root directory of the server
				return 2;
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
	printf("%s\n",dest);
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

void senddata(char *fullpath, int sockdes){
	int fd;
	if((fd = open(fullpath,O_RDONLY)) < 0){
		fprintf(stderr,"open file error");
		exit(1);
	}
	printf("open OK\n");
	char temp[1024];
	int readbyte;
	while((readbyte = read(fd,temp,1024)) > 0){
		printf("%d",readbyte);
		if(write(sockdes,temp,readbyte) != readbyte){
			fprintf(stderr,"write error\n");
			exit(1);
		}
		printf("write OK");
	}
	write(sockdes,"\n",1);
}
