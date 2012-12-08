#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <limits.h>

#ifndef MAX_LEN
#define MAX_LEN 128
#endif

#define NOT_FOUND 404
#define BAD_REQUEST 400
#define PERMISSION 403
#define SERVER_ERROR 500
#define REQUEST_OK 200
#define HEAD 0
#define GET 1
#define VALID 1
#define NOT_VALID 2

/*
* Generate the response time
*/
void responsetime(int client_fd);

/*
* Gnereate the server info
*/
void serverinfo(int client_fd);

/*
* Process the request, generate the response
* @Parameter list:
* @path the request URL after validation
* @method GET,HEAD or POST
* @client_fd, the soceket file descriptor to write the response data
* @protoco, the request protocol
*/
void do_process(char *path, char *method, int client_fd, char *protocol);

/*
* Validation the request URL
* Don't allow the request go out of the root
* No matter the server root or the cgi root or the user root
* If the request gose out of the root, automaticly change the URL under the root.
* @requestdir: the URL from the request
* @rootdir: the server root or the cgi root or the personal folder root, depends on the request type
*/
int validation(char *requestdir, char *rootdir);

/*
* Function used to send data
* Only called when the request is valid and 
* has the permission to read 
* and method is GET
*/
void senddata(int fd, int sockdes);

/*
* The interface used in sws.c
* It only process the normal request type such as:
* /dir/file or /~ccui. 
* CGI request will use another interface
* @client_fd, socket descriptor we will write to 
* @method, request type
* @path, request URL
* @protocol, request prototcol
* @server_root, the server root directory.
*/
void response(int client_fd, char *method, char *path, char *protocol, char *server_root);

/*
* The CGI interface used in sws.c
* Only used when there is a CGI call such as
* /cgi_bin/foo 
* Parameter list is almost the same as the previous function
* The only difference is 
* @cgi_root, indicate the web server's cgi root directory
*/
void cgi_response(int client_fd, char *method, char *path, char *protocol, char *cgi_root);

/*
* Header generater based on different status code
* @client_fd, the web socket descriptor we will write data to
* @status, the HTTP response status code like 200 OK, 404 not found
* @stat_buf, the file or directory infomation
* @dirlen, this parameter only used when we process a directory, 
*          if there is no index.html, we will give the directory list.
	   At this time, the file length in stat_buf cannot be used.
	   What we will do is retrieve the directory to calculate the total length of the file names
           Rewind the directory pointer,
	   Read the directory again and send the file names to the socket
*/
void header(int client_fd,int status,struct stat stat_buf,int dirlen);

void response(int client_fd, char *method, char *path, char *protocol, char *server_root){

	/*
	* Normal case, handling reques like:
	* /dir/subdir/file
	*/
	if(path[2] != '~'){
		char requestdir[MAX_LEN] = "";
		strcat(requestdir,server_root);
		strcat(requestdir,path);
		int validresult = validation(requestdir,server_root);

		/*
		* If the request URL is valid,
		* use it directly
		*/
		if(validresult == VALID){
			do_process(requestdir,method,client_fd,protocol);
		}

		/*
		* If the reqeust URL is not valid,
		* use server_root which is modified in do_process() to be valid;
		*/
		else if(validresult == NOT_VALID){
			do_process(server_root,method,client_fd,protocol);
		}
	}
	
	/*
	* Personal folder request like:
	* /~ccui1/subdir/file
	*/
	else if(path[2] == '~'){

	}
}

int validation(char *requestdir, char *rootdir){

	/*
	* The realpath used to remove the .. and . in a path, to get the real absolute path
	*/
	char path[MAX_LEN];
	realpath(requestdir,path);
	
	//printf("real path: %s\n",path);

	/*
	* Compare the path with root dir,
	* it can be a server root, cgi root or a personal folder root
	*/
	int templen = (strlen(rootdir) < strlen(path) ? strlen(rootdir) : strlen(path));
	int i = 0;
	for(i = 0; i < templen; i++){
		/*
		* If any character is not the same, 
		* the loop ends
		*/
		if(path[i] != rootdir[i])
			break;
	}

	/*
	* If i equals to the length of the root dir,
	* It means that the request dir has the same prefix of the root dir
	* Than it is a valid request
	*/
	if(i == strlen(rootdir)) 
		return VALID;

	/*
	* If i equals to the length of the request dir(path)
	* It means that the request dir is a parent directory of the root dir
	* Than just change the request to root dir.
	* But the return value is NOT_VALID, this means we will use the root dir
	* For this condition, we don't need to modify the root dir.
	*/
	else if(i == strlen(path))
		return NOT_VALID;

	/*
	* If i doesn't equal to length of root dir and request dir
	* And i is less than the length of request dir.
	* This means that from 0 to i in the request dir, it has the same path as the root dir.
	* But from i to the end of the request dir, it has a path that out of the root dir.
	* We automatically concatenate the remaining part of the reqeust dir 
 	* to the server root. We don't allow the request go out of the root,
 	* server root, cgi root and personal folder root
	*/
	else if(i < strlen(path)){ 
		int len = strlen(path) - i + 1;
		char temp[len];
		int j;

		//get the path from i to the end of the path(request dir)
		for(j = 0; j < len; i++,j++)
			temp[j]	= path[i];

		//concatenate it to the root
		strcat(rootdir,"/");
		strcat(rootdir,temp);	

		//then return NOT_VALID
		return NOT_VALID;
	}

	return 0;
}

void cgi_response(int client_fd, char *method, char *path, char *protocol, char *cgi_root){

	
}

void responsetime(int client_fd){
	time_t t;
	char *buf;
	t = time(NULL);
	buf = ctime(&t);
	write(client_fd,"Date: ",6);
	write(client_fd,buf,30);
}

void serverinfo(int client_fd){
	char info[] = "SERVER INFOMATION: simple web server, version 1.0\n";
	write(client_fd,info,sizeof(info));	
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

void do_process(char *path, char *method, int client_fd, char *protocol){

	int status, fd;
	struct stat stat_buf;
	
	// no such file, 404 not found returns
	if(access(path,F_OK) != 0){
		status = NOT_FOUND;
		header(client_fd,status,stat_buf,0);
		exit(0);
	}
	
	// no access permission, 403 permission denied returns
	if(access(path,R_OK) != 0){
		status = PERMISSION;
		header(client_fd,status,stat_buf,0);
		exit(0);
	}

	// stat error, 500 server error returns
	if(stat(path,&stat_buf) == -1){
		status = SERVER_ERROR;
		header(client_fd,status,stat_buf,0);
		exit(0);
	}	

	/*
	* If the request is a file
	*/
	if(!S_ISDIR(stat_buf.st_mode)){
		
		// open error, 500 server error returns
		if((fd = open(path,O_RDONLY)) < 0){
			status = SERVER_ERROR;
			header(client_fd,status,stat_buf,0);
			exit(0);
		}		
	
		// open success, 200 request ok returns
		else{
			status = REQUEST_OK;
			header(client_fd,status,stat_buf,0);
	
			// check the request type, if it is a GET request, we give data back
			if(strcmp(method,"GET") == 0)
				senddata(fd,client_fd);
			exit(0);
		}
		
	}

	/*
	* If the request is a directory
	*/
	if(S_ISDIR(stat_buf.st_mode)){ 
		char temp_dir[MAX_LEN];
		strcpy(temp_dir,path);
		strcat(temp_dir,"/");
		strcat(temp_dir,"index.html");
		
		//There is a index.html in the directory, process the index.html
		if(access(temp_dir,R_OK) == 0){ 
			
			struct stat temp_stat;

			//stat the index.html error, 500 server error back
			if(stat(temp_dir,&temp_stat) == -1){
				status = SERVER_ERROR;
				header(client_fd,status,temp_stat,0);
				exit(0);
			}
		
			//open index.html error, 500 server error back
			if((fd = open(temp_dir,O_RDONLY)) < 0){
				status = SERVER_ERROR;
				header(client_fd,status,temp_stat,0);
				exit(0);
			}
			
			//open OK, 200 request ok back
			else{
				status = REQUEST_OK;
				header(client_fd,status,temp_stat,0);
				
				//check the request type, if it is GET, we give data back
				if(strcmp(method,"GET") == 0)
					senddata(fd,client_fd);
				exit(0);		
			}
		}
		
		//No index.html in the directory, give the file list back
		else{ 
			DIR *dp;
			struct dirent *dir_buf;
			
			//open dir error, 500 server error back
			if((dp = opendir(path)) == NULL){
				status = SERVER_ERROR;
				header(client_fd,status,stat_buf,0);
				exit(0);
			}	

			//open dir success, 200 request ok back
			else{
				status = REQUEST_OK;
				int dirlen = 0;
			
				//calculate the total length of the file names under this directory
				while((dir_buf = readdir(dp)) != NULL)
					dirlen += strlen(dir_buf -> d_name);
			
				//reset the dp to the begining of this directory
				rewinddir(dp);
				header(client_fd,status,stat_buf,dirlen);
				
				//if the method is get, send the file list back
				if(strcmp(method,"GET") == 0){
					while((dir_buf = readdir(dp)) != NULL){
						write(client_fd,dir_buf -> d_name, strlen(dir_buf -> d_name));
						write(client_fd,"\n",1);
					}
				}
				exit(0);
			}
		}
	}
}
		

void header(int client_fd,int status, struct stat stat_buf, int dirlen){
	
	//if the status code is request ok, send the header based on the stat_buf parameter.
	if(status == REQUEST_OK){
		write(client_fd, "http 200 request OK\n", 15);
		responsetime(client_fd);
		serverinfo(client_fd);
		char buf[300];

		//scan the file information into the buf
		int j = sprintf(buf,"Last Modification time : %s",ctime(&stat_buf.st_mtime));		
		j += sprintf(buf+j,"Content type: test/html\n");
		if(!S_ISDIR(stat_buf.st_mode))
			j += sprintf(buf+j,"Content length: %d\n\n",(int)stat_buf.st_size);
		if(S_ISDIR(stat_buf.st_mode))
			j += sprintf(buf+j,"Content length: %d\n\n",dirlen);
	
		write(client_fd,buf,j);
	}

	//if other status code, we don't need stat_buf info, because the last three information will be N/A
	else{
		if(status == SERVER_ERROR)
			write(client_fd,"http 500 server inner error\n",23);
		if(status == NOT_FOUND)
			write(client_fd, "http 404 not found\n",19);
		if(status == BAD_REQUEST)
			write(client_fd,"http 400 bad request\n",16);
		if(status == PERMISSION) 
			write(client_fd, "http 403 permission denied\n", 27);
		responsetime(client_fd);
		serverinfo(client_fd);
		write(client_fd,"Last modification time: N/A\n",28);
		write(client_fd,"content type: N/A\n",18);	
		write(client_fd,"content length: N/A\n",20);
		write(client_fd,"\n",1);
	}
}
