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
#define VERSION_BAD 505
#define HEAD 0
#define GET 1
#define VALID 1
#define NOT_VALID 2
#define CGI_LEN 9

extern int l_flag;
extern int logfd;
extern char loginfo[1024];
extern int d_flag;
char querystring[1024] = "";

typedef struct s_node *Stack;
typedef Stack Next;
typedef Stack Top;

struct s_node{
	char component[MAX_LEN];
	Next next;
};

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

void dir_process(int client_fd, struct stat stat_buf, char *method, char *path);

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
void response(int client_fd, char *method, char *path, char *protocol, char *server_root, char *postdata);

/*
* The CGI interface used in sws.c
* Only used when there is a CGI call such as
* /cgi_bin/foo 
* Parameter list is almost the same as the previous function
* The only difference is 
* @cgi_root, indicate the web server's cgi root directory
*/
void cgi_response(int client_fd, char *method, char *path, char *protocol, char *cgi_root, char *postdata);

void cgi_process(char *requestdir, int client_fd, char *path, char *protocol, char *postdata);
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

void error_content(int client_fd, int status);

void push(Stack node, Top top);
Stack pop(Top top);
void cc_realpath(char *requestdir, char *resolvedpath);
int http_check(char* http);

void response(int client_fd, char *method, char *path, char *protocol, char *server_root, char *postdata){

	/*
	* Normal case, handling reques like:
	* /dir/subdir/file
	*/
	if(path[1] != '~'){
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
	else if(path[1] == '~'){
		char personname[MAX_LEN] = "";
		int i = 2;
		for(i = 2; i < strlen(path); i++){
			if(path[i] != '/')
				personname[i - 2] = path[i];
			else
				break;		
		}	
//		printf("pull the personname : %s out \n",personname);		
		char personroot[MAX_LEN] = "";
		strcpy(personroot,"/home/");
		strcat(personroot,personname);
		strcat(personroot,"/");
		strcat(personroot,"sws/");	
//		printf("personroot: %s \n",personroot);
		
		char requestdir[MAX_LEN] = "";		
		strcpy(requestdir,personroot);
		char temp[MAX_LEN] = "";
		int j;
		for(j = 0; i < strlen(path); i++,j++)
			temp[j] = path[i]; 	
		strcat(requestdir,temp);		
//		printf("requestdir is %s \n",requestdir);

		int validresult = validation(requestdir,personroot);
		if(validresult == VALID)
			do_process(requestdir,method,client_fd,protocol);
		if(validresult == NOT_VALID)
			do_process(personroot,method,client_fd,protocol);
	}
}

int validation(char *requestdir, char *rootdir){

	if(d_flag == 1)
		printf("[INFO]Startging to verify the request URL ...\n");
	/*
	* The realpath used to remove the .. and . in a path, to get the real absolute path
	*/
	char path[MAX_LEN] = "";
	
	if(d_flag == 1)
		printf("[INFO]Translating the relative path to absolute path ... \n");

	cc_realpath(requestdir,path);
	
//	printf("requestdir: %s\n",requestdir);
//	printf("real path after cc_realpath: %s\n",path);

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
	if(i == strlen(rootdir)){ 
		if(d_flag == 1)
			printf("[INFO]Rquest URL is not out of bound\n");
		return VALID;
	}

	/*
	* If i equals to the length of the request dir(path)
	* It means that the request dir is a parent directory of the root dir
	* Than just change the request to root dir.
	* But the return value is NOT_VALID, this means we will use the root dir
	* For this condition, we don't need to modify the root dir.
	*/
	else if(i == strlen(path)){
		if(d_flag == 1)
			printf("[INFO]Request URL is out of root, automatically transfer URL back to root ...\n");
		return NOT_VALID;
	}

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
			
		/*
		* At position i, path and rootdir won't have the same charactor
		* So, if path[i] is /, then rootdir is not /
		* And if path [i] is /, it means such condition happened:
		* the root dir is /home/cc/homework631/....
		* the path dir is /home/cc/ho/....
		* We cannot take the last / as the seperation.
		* We need to find the previous / as the seperation. 
		* From there to the end of the path, are the remaining path we need.
		* So only path[i] and rootdir[i] are both /, we take that i position as the starting point.
		* We need to fine the first / from there, take all the rest path.
		* Concatenate the rest path to the root dir.
		*/
		for(;i >= 0; i--){
			if(path[i] == '/' && rootdir[i] == '/')
				break;
		}

		int len = strlen(path) - i + 1;
		char temp[len];
		int j;

		//get the path from i to the end of the path(request dir)
		for(j = 0; j < len; i++,j++)
			temp[j]	= path[i];

		//concatenate it to the root
		strcat(rootdir,temp);	
//		printf("trace the rootdir after validation: %s\n",rootdir);		
		
		if(d_flag == 1)
			printf("[INFO]Request URL is out of root, automatically transfer URL back to root ...\n");
		//then return NOT_VALID
		return NOT_VALID;
	}

	return 0;
}

void cgi_response(int client_fd, char *method, char *path, char *protocol, char *cgi_root, char *postdata){
	
	/*
	* Get the path after the /cgi_bin/
	* For example:
	* the request is /cgi_bin/subdir/hello
	* We get the /subdir/hello part
	*/
	int templen = strlen(path) - CGI_LEN, i,j;
	char tempdir[templen + 1];
	for(i = 0; i < templen + 1; i++)
		tempdir[i] = path[CGI_LEN + i];
	char requestdir[MAX_LEN];
	strcpy(requestdir,cgi_root);
	strcat(requestdir,"/");
	
	/*
	* If it is a GET request, the input value of the request may be after the path.
	* Here we check that
	* Format: URL?name=value&name=value 
	* We need the string after ?
	*/
	
	if(strcmp(method,"GET") == 0){
		for(i = 0; i < strlen(tempdir); i++){
			if(tempdir[i] == '?')
				break;
		}

		for(j=0; i<strlen(tempdir);i++,j++)
			querystring[j] = tempdir[i + 1];
		
		tempdir[i - j] = 0;
	}
	
	/*
	* Make the full requestdir path
	*/
	strcat(requestdir,tempdir);
	
	int validresult = validation(requestdir,cgi_root);
	
	/*
	* If the request URL is valid, use the reques dir directly
	*/
	if(validresult == VALID){
		cgi_process(requestdir,client_fd,method,protocol,postdata);
	}
		
	/*
	* If the request URL is not valid, use the cgi_root which has been modified in the validation function
	*/
	if(validresult == NOT_VALID){
		cgi_process(cgi_root,client_fd,method,protocol,postdata);	
	}	
}

void responsetime(int client_fd){
	time_t t;
	char *buf;
	t = time(NULL);
	buf = ctime(&t);
	write(client_fd,"Date: ",6);
	write(client_fd,buf,strlen(buf));
}

void serverinfo(int client_fd){
	char info[] = "SERVER INFOMATION: simple web server, version 1.0\n";
	write(client_fd,info,strlen(info));	
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

	//wrong HTTP version 505 error return
	int version = http_check(protocol);
	if(version == 0){
		status = VERSION_BAD;
		header(client_fd,status,stat_buf,0);
		exit(0);
	}

	//These files doesn't support POST method
	if(strcmp(method,"POST") == 0){
		status = 405;
		header(client_fd,status,stat_buf,0);
		exit(0);
	}	

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
	if(S_ISDIR(stat_buf.st_mode)) 
		dir_process(client_fd,stat_buf,method,path);	
}
		
void cgi_process(char *path, int client_fd, char *method, char *protocol, char *postdata){
	
	int status;
	struct stat stat_buf;
	
	//wrong HTTP version 505 error return
	int version = http_check(protocol);
	if(version == 0){
		status = VERSION_BAD;
		header(client_fd,status,stat_buf,0);
		exit(0);
	}

	//Check the existance of the request file
	if(access(path, F_OK) != 0){
		status = NOT_FOUND;
		header(client_fd,status,stat_buf,0);
		exit(0);
	}

	//Stat to get the file info
	if(stat(path,&stat_buf) == -1){ 
		status = SERVER_ERROR;
		header(client_fd,status,stat_buf,0);
		exit(0);
	}

	//If request a execution file, we call the execle to execute the file,	
	if(!S_ISDIR(stat_buf.st_mode)){
		
		//check the execution permission
		if(access(path,X_OK) != 0){
			status = PERMISSION;
			header(client_fd,status,stat_buf,0);
			exit(0);
		}

		else{ //do execution
			
			/*
			* If it is a GET CGI request.
			* The data send to the CGI is stored in the environment QUERY_STRING.
			*/
			if(strcmp(method,"GET") == 0){
				int pid;
				
				write(client_fd,"HTTP 200 request OK\n",20);
				strcat(loginfo,"[INFO]HTTP 200 request OK\n");
				
				if(d_flag == 1)
					printf("%s",loginfo);

				write(logfd,loginfo,strlen(loginfo));

				responsetime(client_fd);
				serverinfo(client_fd);

				if((pid = fork()) < 0){
					status = SERVER_ERROR;
					header(client_fd,status,stat_buf,0);
					exit(0);
				}

				if(pid == 0){ //fork a child to do execution
					char temp[MAX_LEN] = "";
					sprintf(temp,"QUERY_STRING=%s",querystring);	
					char *env_init[] = {"PATH=/tmp",temp,NULL};				
					//redirect the output of the execution result to the socket descriptor
					dup2(client_fd,STDOUT_FILENO);	
					if(execle(path,"",(char *)0,env_init) < 0){
						status = SERVER_ERROR;
						header(client_fd,status,stat_buf,0);
						exit(0);	
					}
				}
			}
			
			/*
			* If it is POST CGI request.
			* Then, the the data is send to the child process
			* The CGI script will read the data from standard in stream
			* The data length is stored in the environment CONTENT_LENGTH;
			*/
			if(strcmp(method,"POST") == 0){
				int pid;
			
				int fd[2];
				/*
				* Create a pipe, the parent process send the postdata to the pipe
				* The child process read the data from the pipe.
				* Redirect the fd[0] of this pipe to the stdin.
				* The CGI script will read the postdata from stdin.
				*/
				if(pipe(fd) < 0){
					status = SERVER_ERROR;
					header(client_fd,status,stat_buf,0);
					exit(0);
				}
				/*
				* CGI execution success header
				*/
				write(client_fd,"HTTP 200 request OK\n",20);
				strcat(loginfo,"[INFO]HTTP 200 request OK\n");

				/*
				* Write the post data to the child's stdin
				* The CGI script can use it.
				*/
				write(fd[1],postdata,strlen(postdata));
				
				/*
				* Print the debuf info and log the info into file
				*/
				if(d_flag == 1)
					printf("%s",loginfo);
				write(logfd,loginfo,strlen(loginfo));
				
				responsetime(client_fd);
				serverinfo(client_fd); 
				
				if((pid = fork()) < 0){
					status = SERVER_ERROR;
					header(client_fd,status,stat_buf,0);
					exit(0);
				}

				if(pid == 0){ //fork a child to do execution
					char temp[MAX_LEN] = "";
					sprintf(temp,"CONTENT_LENGTH=%d",strlen(postdata));
					char *env_init[] = {"PATH=/home/cc/homework_CS631/final631/sws/cgi_bin",temp,NULL};				

					close(fd[1]);
					/*
					* Redirect the stdin to the pipe's read end  
					*/
					dup2(fd[0],STDIN_FILENO);
					/*
					* redirect the output of the execution result to the socket file descriptor
					*/
					dup2(client_fd,STDOUT_FILENO);
					if(execle(path,"",(char *)0,env_init) < 0){
						perror(strerror(errno));
						status = SERVER_ERROR;
						header(client_fd,status,stat_buf,0);
						exit(0);	
					}
				}
			}
		}	
	}
	else //if request a dir cgi_bin or under it, the same as normal request
		dir_process(client_fd,stat_buf,method,path);	
}

void header(int client_fd,int status, struct stat stat_buf, int dirlen){
	
	//if the status code is request ok, send the header based on the stat_buf parameter.
	if(status == REQUEST_OK){
		write(client_fd, "HTTP 200 request OK\n", 20);
		strcat(loginfo,"[INFO]HTTP status code: 200\n");
		responsetime(client_fd);
		serverinfo(client_fd);
		char buf[300] = "";

		//scan the file information into the buf
		int j = sprintf(buf,"Last-Modification-Time : %s",ctime(&stat_buf.st_mtime));		
		j += sprintf(buf+j,"Content-Type: text/html\n");
		if(!S_ISDIR(stat_buf.st_mode)){
			j += sprintf(buf+j,"Content-Length: %d\n\n",(int)stat_buf.st_size);
			char temp[100] = "";
			sprintf(temp,"[INFO]Response Content-Length : %d\n\n",(int)stat_buf.st_size);
			strcat(loginfo,temp);
		}
		if(S_ISDIR(stat_buf.st_mode)){
			j += sprintf(buf+j,"Content-Length: %d\n\n",dirlen);
			char temp[100] = "";
			sprintf(temp,"[INFO]Response Content-Length : %d\n\n",dirlen);
			strcat(loginfo,temp);
		}
	
		if(l_flag == 1)
			write(logfd,loginfo,strlen(loginfo));

		if(d_flag == 1){
			loginfo[strlen(loginfo) - 1] = 0;
			printf("%s",loginfo);
		}
		write(client_fd,buf,j);
	}

	//if other status code, we don't need stat_buf info, because the last three information will be N/A
	else{
		if(status == 405){
			write(client_fd,"HTTP 405 method not allowed\n",28);
			strcat(loginfo,"[INFO]HTTP status code: 405\n");
		}
		if(status == 411){
			write(client_fd,"HTTP 411 length required\n",25);
			strcat(loginfo,"[INFO]HTTP status code: 411\n");
		}
		
		if(status == 501){
			write(client_fd,"HTTP 501 not implemented\n",25);
			strcat(loginfo,"[INFO]HTTP status code: 501\n");
		}
			
		if(status == VERSION_BAD){
			write(client_fd,"HTTP 505 version not support\n",29);
			strcat(loginfo,"[INFO]HTTP status code 505\n");
		}

		if(status == SERVER_ERROR){
			write(client_fd,"HTTP 500 server inner error\n",28);
			strcat(loginfo,"[INFO]HTTP status code: 500\n");
		}
		if(status == NOT_FOUND){
			write(client_fd, "HTTP 404 not found\n",19);
			strcat(loginfo,"[INFO]HTTP status code: 404\n");
		}
		if(status == BAD_REQUEST){
			write(client_fd,"HTTP 400 bad request\n",21);
			strcat(loginfo,"[INFO]HTTP status code: 400\n");
		}
		if(status == PERMISSION){ 
			write(client_fd, "HTTP 403 permission denied\n", 27);
			strcat(loginfo,"[INFO]HTTP status code: 403\n");
		}
		responsetime(client_fd);
		serverinfo(client_fd);
		write(client_fd,"Last-Modification-Time: N/A\n",28);
		write(client_fd,"Content-Type: text/html\n",24);	

		char temp[30] = "";
		char detail[100]  = "";
		if(status == 400){
			strcpy(temp,"Bad Request");
			strcpy(detail,"\n");
		}

		if(status == 403){
			strcpy(temp,"Permission denied");
			strcpy(detail,"\nHave no access permission to the request item\n");
		}

		if(status == 404){
			strcpy(temp,"Not Found");
			strcpy(detail,"\nThe request file is not found on this server\n");
		}

		if(status == 405){
			strcpy(temp,"Method not allowed");
			strcpy(detail,"\nThe file you request does not allow a POST method on it\n");
		}

		if(status == 411){
			strcpy(temp,"length required");
			strcpy(detail,"\nPOST request need a Content-Length: in the reqeust header\n");
		}

		if(status == 500){
			strcpy(temp,"server inner error");
			strcpy(detail,"\nError occur inside the server, cannot response to the request\n");
		}

		if(status == 501){
			strcpy(temp,"not implemented");
			strcpy(detail,"\nMethod is not imeplmented, only support GET POST HEAD\n");
		}

		if(status == 505){
			strcpy(temp,"version not support");
			strcpy(detail,"\nThe given version of HTTP is not supported on this server\n");
		}
		char buf[300] = "";
		int len = 0;
		len = sprintf(buf,"<HTML>\n<H4> HTTP %d %s error</H4>",status,temp);
		len += sprintf(buf+len," %s</HTML>\n", detail);

		char contentlen[300] = "";
		sprintf(contentlen,"Content-Length:%d\n\n",len);
		
		/*
		* Send the error content length header to client side
		*/
		write(client_fd,contentlen,strlen(contentlen));

		/*
		* Record the log information
		*/
		strcat(loginfo,"[INFO]");
		strcat(loginfo,contentlen);

		/*
		* Write the actual error content to the client side
		*/
		write(client_fd,buf,len);

		/*
		* Log the request info
		*/
		if(l_flag == 1)
			write(logfd,loginfo,strlen(loginfo));

		/*
		* Show the request info under debug mode on terminal
		*/
		if(d_flag == 1){
			loginfo[strlen(loginfo) - 1] = 0;
			printf("%s",loginfo);
		}
	}
}

void cc_realpath(char *path, char *resolvedpath){
	
	//generate a stack to store each component of the path.	
	Top c_stack;
	c_stack = (Top)malloc(sizeof(struct s_node));	
	c_stack -> next = NULL;
	
	char remaining[MAX_LEN];	
	strcpy(remaining,path);
	int templen = strlen(remaining);
	
	/*
	* count is used to count the number of .. in the path
	* j is used to count the length of each component
	*/
	int count = 0, j = 1, i;

	/*
	* We traverse the path from end to beginning
	* Each time we met a /, we know that from that position, 
	* in j length, they all belong to a component.
	* Then, we check this component,
	* If it is a .., we increase the count number,
	* If it is not a .., we check the count number,
	* 	If the count is 0, that means we can push this component to the stack
	* 	If the count is greater than 0, that means this component will be remove by a .. operation. So do nothing, just minuse 1 from count.
	* If it is a ., because we have pushed it on to the stack at previous operation, we need to pop it out.
	* After this component is done, we reset j to 1.(j is 1 initially because each component at least has one character that is /) 
	*/
	for(i = templen; i >= 0; i --){
		
		//Get the length of each component
		if(remaining[i] != '/')
			j++;
		
		//We found the seperation of this component, process this component
		else{
			int m = 0;
			Stack node;
			node = (Stack)malloc(sizeof(struct s_node));

			//store the string in the stack node.
			for(m = 0; m < j; m++)
				node -> component[m] = remaining[i+m];
				
		//	node ->	component[m+1] = 0;
		//	printf("each component %s \n", node -> component);

			//If it is not a ..
			if(strcmp(node -> component, "/..") != 0){
				
				//if count is 0, push onto stack
				if(count == 0)	
					push(node,c_stack);
				//if count is greater than 0, minuse 1.
				if(count > 0)
					count --;
			}
			//If it is a .., increase count.
			else
				count ++;
		
			//if it is a ., remove it from the stack, because . has no effection in a path.
			if(strcmp(node -> component, "/.") == 0)
				free(pop(c_stack));

			//reset j to 1
			j = 1;
		}
	}

	/*
	* After processing all the components
	* The remaining components in the stack will make the real path of the given path.
	* Traverse the stack and concatenate them together.
	*/
	while(c_stack -> next != NULL){
		Stack tempstack = pop(c_stack);
		strcat(resolvedpath,tempstack -> component);
		//printf("trace the result path %s\n",resolvedpath);
		free(tempstack);
	}
}

//push into stack
void push(Stack node, Top top){
	
	node -> next = top -> next;
	top -> next = node;

}

//pop out stack
Stack pop(Top top){

	Stack temp;
	temp = top -> next;
	top -> next = top -> next -> next;
	return temp;
	
}

void dir_process(int client_fd, struct stat stat_buf,char *method, char *path){
	int status;
	char temp_dir[MAX_LEN];
	strcpy(temp_dir,path);
	strcat(temp_dir,"/");
	strcat(temp_dir,"index.html");
		
	//There is a index.html in the directory, process the index.html
	if(access(temp_dir,R_OK) == 0){ 
		int fd;	
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
			int count = 0;
		
			//calculate the total length of the file names under this directory
			while((dir_buf = readdir(dp)) != NULL){
				dirlen += strlen(dir_buf -> d_name);
				count++;
			}
		
			//reset the dp to the begining of this directory
			rewinddir(dp);
			dirlen = dirlen + 9*count;
			dirlen += 15;
			header(client_fd,status,stat_buf,dirlen);
			
			//if the method is get, send the file list back
			if(strcmp(method,"GET") == 0){
				write(client_fd,"<HTML>\n",7);
				while((dir_buf = readdir(dp)) != NULL){
					write(client_fd,"<br>",4);
					write(client_fd,dir_buf -> d_name, strlen(dir_buf -> d_name));
					write(client_fd,"</br>",5);
				}
				write(client_fd,"</HTML>\n",8);
			}
			exit(0);
		}
	}
}

int http_check(char *http){
	if(strcmp(http,"") == 0)
		strcpy(http,"HTTP/1.0");
	if(strcmp(http,"HTTP/1.0") != 0 && strcmp(http,"HTTP/1.1") != 0)
		return 0;
	return 1;
}
