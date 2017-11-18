/**************************************************************************
****Author: Shekhar 
****C socket server example, handles multiple clients using threads
****reference:http://www.binarytides.com/server-client-example-c-sockets-linux/
****from client type the below command in terminal
**** (echo -en "GET /index.html HTTP/1.1\r\nHost: localhost\r\n"; sleep 10) | telnet 127.0.0.1 8888
**************************************************************************/

#include <stdio.h>
#include <string.h> //strlen
#include <stdlib.h> //strlen
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <unistd.h> //write
#include <pthread.h> //for threading , link with lpthread
#include <errno.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>
#include <semaphore.h>

/*** #defines ***/
//#define DEBUG_ON 1
#ifdef DEBUG_ON
#define DEBUG(X) X; fflush(stdout)
#else
#define DEBUG(X)
#endif

#define LISTEN "Listen"
#define DOC_ROOT "DocumentRoot"
#define DIR_INDEX "DirectoryIndex"
#define CONN_KEEP_LIVE "Keep-Alive"
#define CONN_TYPE "Content-Type"

#define MAXLIMIT 2048
#define MINLIMIT 200
#define GET_COMMAND "GET"
#define POST_COMMAND "POST"
#define HOST_PARAM "Host"
#define CONNECTION_PARAM "Connection"
#define KEEP_ALIVE "alive"
#define HTTP_1_1_VERSION "HTTP/1.1"
#define HTTP_1_0_VERSION "HTTP/1.0"
#define WEBCONFIG "/home/shekhar/Netsys/p2/P2/ws.conf"
#define DIRECTORY_TO_SEARCH "/home/shekhar/Netsys/p2/P2/www" //"."
#define REASON_INVALID_HTTP_VERSION "Reason: Invalid HTTP-Version:"
#define REASON_INVALID_METHOD "Reason: Invalid Method:"
#define REASON_INVALID_URL "Reason: Invalid URL:"
#define REASON_FILE_NOT_FOUND "Reason URL does not exist :"
#define REASON_500_SERVER_ERROR ": cannot allocate memory"
#define CONNECTION_KEEP_ALIVE "Connection: Keep-Alive"
#define CONNECTION_CLOSE "Connection: Close"
#define OK_200 "200 OK"
#define BAD_500_SERVER_ERROR " 500 Internal Server Error"
#define BAD_REQUEST_400 " 400 Bad Request"
#define FILE_TYPE_NOT_FOUND_501 " 501 Not Implemented"
#define FILE_NOT_FOUND_404 " 404 Not Found"
#define DELIMITER "\r\n"
#define PRINT_DECORATE printf("%.*s\n", 10, "**")

/***DataStructure Declaration ***/
typedef struct tcpRecvPacketInfo {
    char* fileName;
    char* httpVersion;
    char* hostName;
    int connectionALiveFlag;
} tcpRecvPacketInfo;

typedef struct tcpReplyPacketInfo {
    char httpMethod[MINLIMIT];
    char fileName[MINLIMIT];
    char fileType[MINLIMIT];
    char httpVersion[MINLIMIT];
    char hostName[MINLIMIT];
    int  connectionAliveFlag;
    int  contentLength;
    int  isPostMethod;
    int  postContentLength;
    char postContent[MAXLIMIT]; 
    int  errorFlag;
    char errorMessage[MINLIMIT];
    char errorReason[MINLIMIT];
} tcpReplyPacketInfo;

/***Global Variables ***/
unsigned int _global_PortNumber = 0;
char _global_DocumentRoot[MINLIMIT];
char * _global_DirectoryIndex[MINLIMIT];
unsigned int _global_KeepAliveTime = 0;
char* _global_Content_Type[MINLIMIT][2];
sem_t AcceptConnection;

/***Declaration of Functions***/
void* connection_handler(void*); //the thread function
void parseHttp(char* httpRequest, tcpReplyPacketInfo* replyPacketInfo);
void show_dir_content(char* path, char* fileName, int* fileExists);
char* getFileNameFromDirectoryString(char* token);
int readWebConfig();
int compare_filename_ext(const char* filename, char* replyfileType);
/***********************************************************************************
*** getFileNameFromDirectoryString function gets the fileName from the lengthy DIrectory names 
*** ex : /images/fileName.txt this function gets filename fileName.txt
***********************************************************************************/
char* getFileNameFromDirectoryString(char* token)
{
    char* temp;
    char* tokenCopy;
    tokenCopy = (char*)malloc(strlen(token));
    strcpy(tokenCopy, token);

    DEBUG(printf("this is before first substring: %s\n", token));
    temp = strtok(token, "/");
    DEBUG(printf("this is after first substring: %s\n this is the token now : %s\n", temp, token));
    while (temp != NULL) {
        if (temp != NULL) {
            memset(tokenCopy, '\0', strlen(tokenCopy));
            strcpy(tokenCopy, temp);
            DEBUG(printf("\nthei is the string: %s\n", tokenCopy));
        }
        temp = strtok(NULL, "/");
    }
    return tokenCopy;
}

/***********************************************************************************
*** show_dir_content : A recursive function to search a file of interest recursively in a given directory
*** reference : https://stackoverflow.com/questions/4204666/how-to-list-files-in-a-directory-in-a-c-program
*** path : the the path in which the search  has to be done
*** fileName: the file Name to search
*** fileExixts: is a global flag which will be updated respective to the existence of the file
***********************************************************************************/

void show_dir_content(char* path, char* fileName, int* fileExists)
{
    DIR* d = opendir(path); // open the path
    if (d == NULL)
        return; // if was not able return
    struct dirent* dir; // for the directory entries
    while ((dir = readdir(d)) != NULL) // if we were able to read somehting from the directory
    {
        if (dir->d_type != DT_DIR) // if the type is not directory see if the file we are searching is present
        {
            if (!(strcmp(dir->d_name, fileName))) {
                DEBUG(printf("FOund THe file %s\n", dir->d_name));
                char buffer[MINLIMIT];
                memset(buffer, '\0', MINLIMIT);
                sprintf(buffer, "%s/%s", path, fileName);
                strcpy(fileName, buffer);
                *fileExists = 1;
            }
        }
        else if (dir->d_type == DT_DIR && strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) // if it is a directory
        {
            char d_path[255]; // here I am using sprintf which is safer than strcat
            sprintf(d_path, "%s/%s", path, dir->d_name);
            show_dir_content(d_path, fileName, fileExists); // recall with the new path
        }
    }
    closedir(d); // finally close the directory
}

/***********************************************************************************
*** parseHttp function : is used to parse the HTTP request and check if method,filename, file extensions are all appropriate
*** httpRequest1 : is the httprequest 
*** replyPacketInfo: is pointer type of tcpReplyPacketInfo, to which the respective parsed data will be updated
***********************************************************************************/
void parseHttp(char* httpRequest1, tcpReplyPacketInfo* replyPacketInfo)
{
    char* token;
    printf("++++++++++++++ THe str i got is this : %s\n", httpRequest1);
    char * httpRequest = (char *)malloc(strlen(httpRequest1));
    strcpy(httpRequest,httpRequest1);
    replyPacketInfo->isPostMethod=0;
    /* get the first token */
    token = strtok(httpRequest, DELIMITER);
    if (token != NULL) {
        if ((strstr(token, GET_COMMAND) != NULL) || (strstr(token, POST_COMMAND) != NULL)) {
	    if(strstr(token, POST_COMMAND) != NULL)replyPacketInfo->isPostMethod= 1;
            // this itself takes care of trailing and leading zeros
            DEBUG(printf("The token: %s", token));
            if (sscanf(token, "%s %s %s", replyPacketInfo->httpMethod, replyPacketInfo->fileName, replyPacketInfo->httpVersion) < 1) {
                printf("Invalid Entry\n"); //501 error
            }
            if(strlen(replyPacketInfo->fileName)<4)
	    sprintf(replyPacketInfo->fileName, "/%s",_global_DirectoryIndex[0]);
            if (!((!(strcmp(replyPacketInfo->httpVersion, HTTP_1_0_VERSION))) || (!(strcmp(replyPacketInfo->httpVersion, HTTP_1_1_VERSION))))) 		    {
                DEBUG(printf("HERE1\n"));
                replyPacketInfo->errorFlag = 1;
                strcpy(replyPacketInfo->errorMessage, BAD_REQUEST_400);
                sprintf(replyPacketInfo->errorReason, "%s %s %s", BAD_REQUEST_400, REASON_INVALID_HTTP_VERSION, replyPacketInfo->httpVersion);
                return;
            }
            else if (compare_filename_ext(replyPacketInfo->fileName, replyPacketInfo->fileType) == 0) {
                DEBUG(printf("HERE2\n"));
                int fileExists = 0;
                DEBUG(printf("HERE4\n"));
                FILE* fileOpen = NULL;
                printf("get req file : %s\n", replyPacketInfo->fileName);
                char* tempString;
                tempString = (char*)malloc(strlen(replyPacketInfo->fileName));
                strcpy(tempString, replyPacketInfo->fileName);
                memset(replyPacketInfo->fileName, '\0', MINLIMIT);
                sprintf(replyPacketInfo->fileName, "%s%s", _global_DocumentRoot, tempString);
                free(tempString);
                printf("changed now %s\n", replyPacketInfo->fileName);
                fileOpen = fopen(replyPacketInfo->fileName, "r"); //501error
                DEBUG(printf("HERE4.1\n"));
                fflush(stdout);
                if (fileOpen == NULL) {
                    printf("file error  : error %d\n", errno);
                    if (errno == ENOENT) {
                        printf("inside the error\n");
                        replyPacketInfo->errorFlag = 1;
                        DEBUG(printf("HERE4.11\n"));
                        strcpy(replyPacketInfo->errorMessage, FILE_NOT_FOUND_404);
                        DEBUG(printf("HERE4.12\n"));
                        sprintf(replyPacketInfo->errorReason, "%s %s %s", FILE_NOT_FOUND_404, REASON_FILE_NOT_FOUND, strrchr(replyPacketInfo->fileName, '/'));
                        DEBUG(printf("HERE4.13\n"));
                        return; //what if the file was index.html - not founf - we cant continue
                    }
                }
                fseek(fileOpen, 0, SEEK_END);
                int fileSize = ftell(fileOpen);
                DEBUG(printf("HERE4.2\n"));
                fflush(stdout);
                rewind(fileOpen);
                fclose(fileOpen);
                DEBUG(printf("HERE4.3\n"));
                fflush(stdout);
                if (fileSize < 0) {
                    replyPacketInfo->errorFlag = 1;
                    strcpy(replyPacketInfo->errorMessage, BAD_500_SERVER_ERROR);
                    sprintf(replyPacketInfo->errorReason, "%s %s", BAD_500_SERVER_ERROR, REASON_500_SERVER_ERROR);
                    return;
                }
                else {
                    DEBUG(printf("HERE5\n"));
                    replyPacketInfo->contentLength = fileSize;
                }
                /* }*/
            }
            else {
                printf("**************no file extension as such sorry\n");
                replyPacketInfo->errorFlag = 1;
                strcpy(replyPacketInfo->errorMessage, FILE_TYPE_NOT_FOUND_501);
                sprintf(replyPacketInfo->errorReason, "%s %s", FILE_TYPE_NOT_FOUND_501, replyPacketInfo->fileName); //replyPacketInfo->httpMethod);
                return;
            }
        }
        else {
            DEBUG(printf("HERE6\n"));
            replyPacketInfo->errorFlag = 1;
            strcpy(replyPacketInfo->errorMessage, FILE_TYPE_NOT_FOUND_501);
            sprintf(replyPacketInfo->errorReason, "%s %s", FILE_TYPE_NOT_FOUND_501, replyPacketInfo->httpMethod);
            return;
        }
        int HostExistsFlag = 1, ContentLengthFlag =0 , PostContentFlag= 0;
        int ConnectionExistsFlag = 0, dataCounter= 0;
	char * tempCounter = (char *)malloc(MAXLIMIT);
        tempCounter = replyPacketInfo->postContent;
        /* walk through other tokens */
        while (token != NULL) {
            DEBUG(printf("HERE7.1\n"));
            DEBUG(printf("toekn:%s\n", token));
            token = strtok(NULL, DELIMITER);
	    if(token==NULL)
               break;
            DEBUG(printf("HERE7.2\n"));
            DEBUG(printf("toekn:%s\n", token));
            if (!HostExistsFlag) {
                DEBUG(printf("HERE7.3\n"));
                if (strstr(token, HOST_PARAM) != NULL) {
                    DEBUG(printf("HERE7.4\n"));
                    HostExistsFlag = 1;
                }
            }
            if (!ConnectionExistsFlag) {
                DEBUG(printf("HERE7.5\n"));
                DEBUG(printf("toekn:%s\n", token));
                fflush(stdout);
                if ((strstr(token, CONNECTION_PARAM) != NULL) && (strstr(token, KEEP_ALIVE) != NULL)) {
                    DEBUG(printf("HERE8\n"));
                    ConnectionExistsFlag = 1;
                    replyPacketInfo->connectionAliveFlag = 1;
                }
            }
 	    /*if(PostContentFlag)
 	    {
		 DEBUG(printf("HERE9.5\n"));
		if(dataCounter< replyPacketInfo->postContentLength)
		{
		DEBUG(printf("replyPacketInfo->postContentLength :%d",replyPacketInfo->postContentLength));
		 DEBUG(printf("before -> dataCounter :%d, tempCounter : %s\n",dataCounter,tempCounter));
		   strncpy(tempCounter, token, strlen(token));
		   tempCounter+dataCounter;
		   dataCounter += strlen(token);
		DEBUG(printf("before -> dataCounter :%d, tempCounter : %s\n",dataCounter,tempCounter));
	        }else
		{
		   PostContentFlag= 0;
		}
            }*/
	    if (replyPacketInfo->isPostMethod && !ContentLengthFlag) {
                DEBUG(printf("HERE8.5\n"));
                DEBUG(printf("toekn:%s\n", token));
                fflush(stdout);
		DEBUG(printf("The value for content lenght :%s", strstr(token, "Content-Length:")));
                if ((strstr(token, "Content-Length:") != NULL) || (strstr(token, "content-length:") != NULL)) {
                    DEBUG(printf("HERE8.6\n"));
		    sscanf(token, "%*s %d", &replyPacketInfo->postContentLength);
		    printf("the token data for post lenth : %s %d",token, replyPacketInfo->postContentLength);
		    PostContentFlag = 1;
                    ContentLengthFlag = 1;
                }
            }

            if (ConnectionExistsFlag && HostExistsFlag) {
                if (replyPacketInfo->isPostMethod)
		{
		    if(ContentLengthFlag)break;
		}else break;
            }
        }
	if((!replyPacketInfo->errorFlag)&&(replyPacketInfo->isPostMethod)&&(replyPacketInfo->postContentLength!=0))
	{
	    char * temp = (char *) malloc(replyPacketInfo->postContentLength);
	    if(((temp = strstr(httpRequest1, "\r\n\r\n"))!=NULL)||((temp = strstr(httpRequest1,"\n\n"))!=NULL))
	    {
		sprintf(replyPacketInfo->postContent, "<pre><h1>POST DATA</h1>%s</pre>",temp);
		printf("The data is :%s\n", replyPacketInfo->postContent);
		sprintf(replyPacketInfo->postContent, "<script type=\"text/javascript\">alert(0);$(document).ready(function(){$('<font color=\"white\"><pre ><h1>POST DATA</h1>%s</pre></font>').prependTo('body');});</script>", temp);
		replyPacketInfo->contentLength+= strlen(replyPacketInfo->postContent);
	    }
	}
    }
    else {
        replyPacketInfo->errorFlag = 1;
        strcpy(replyPacketInfo->errorMessage, FILE_TYPE_NOT_FOUND_501);
        sprintf(replyPacketInfo->errorReason, "%s %s", FILE_TYPE_NOT_FOUND_501, replyPacketInfo->fileName);
        return;
    }
    DEBUG(printf("HERE9\n"));
    printf("######################\nThe post data %s\n######################\n", replyPacketInfo->postContent);
    return;
}

/***********************************************************************************
*** compare_filename_ext function : compares if the file extension exists in the ws.conf; replies -1 if not found
*** filename : the filename with extension  
*** replyfileType:  will be updated with the appropirate ws.conf filetype for browsers if the file exists
***********************************************************************************/
int compare_filename_ext(const char* filename, char* replyfileType)
{
    const char* dot = strrchr(filename, '.');
    DEBUG(printf("*************************************\nfilename :%s\n", filename));
    DEBUG(printf("dot name :%s\n", dot));
    if (!(!dot || dot == filename)) {
        DEBUG(printf("row size:%d\n", (int)(sizeof(_global_Content_Type) / sizeof(_global_Content_Type[0]))));
        for (int i = 0; i < (sizeof(_global_Content_Type) / sizeof(_global_Content_Type[0])), _global_Content_Type[i][0] != NULL; i++) {
            if (strcmp(_global_Content_Type[i][0], dot) == 0) {

                DEBUG(printf("value : %s\n", _global_Content_Type[i][0]));
                strcpy(replyfileType, _global_Content_Type[i][1]);
                return 0;
            }
        }
    }
    return -1;
}

/***********************************************************************************
*** readWebConfig function : parses the ws.conf file and put the extracted values to global variables
*** the global variables are used to configure the webserver; thi fuction is called once after the main starts
***********************************************************************************/

int readWebConfig()
{
    FILE* webConfig;

    webConfig = fopen(WEBCONFIG, "rb");
    if (webConfig == NULL) {
        perror("WebConfig File error:");
    }
    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    char temp[MINLIMIT] = { 0 };
    int counter = 0, content_typeflag = 0;
    while ((read = getline(&line, &len, webConfig)) != -1) {
        DEBUG(printf("Retrieved line of length %zu :\n", read));
        DEBUG(printf("%s", line));
        if (strstr(line, LISTEN) != NULL) //Listen
        {
            sscanf(line, "%*s %d", &_global_PortNumber);
            DEBUG(printf("_global_PortNumber:%d\n", _global_PortNumber));
            //content_typeflag = 0;
        }
        if (strstr(line, DOC_ROOT) != NULL) //DocumentRoot
        {
            sscanf(line, "%*s %s", _global_DocumentRoot);
            DEBUG(printf("_global_DocumentRoot:%s\n", _global_DocumentRoot));
            //content_typeflag = 0;
        }
        if (strstr(line, DIR_INDEX) != NULL) //DirectoryIndex
        {
	    char * token; 
   	    token = strtok(line, " ");
	    int counter=0;
	    while (token != NULL) {
		    DEBUG(printf("HERE7.1\n"));
		    DEBUG(printf("toekn:%s\n", token));
		    token = strtok(NULL, " ");
		    if(token!=NULL)
		    {
			_global_DirectoryIndex[counter] = (char *)malloc(strlen(token));
		        sprintf(_global_DirectoryIndex[counter],"%s", token);
		        counter++;
		    }
	    }
            DEBUG(printf("_global_DirectoryIndex:%s\n", _global_DirectoryIndex[0]));
            //content_typeflag = 0;
        }
        if (strstr(line, CONN_KEEP_LIVE) != NULL) //Keep-Alive
        {
            sscanf(line, "%*s %*s %d", &_global_KeepAliveTime); //Keep-Alive time 10
            content_typeflag = 0;
            DEBUG(printf("_global_KeepAliveTime:%d\n", _global_KeepAliveTime));
        }

        if (content_typeflag) {
            sscanf(line, "%s %s", (_global_Content_Type[counter][0] = (char*)malloc(strlen(line))), (_global_Content_Type[counter][1] = (char*)malloc(strlen(line))));
            DEBUG(printf("_global_Content_Type:%s %s\n", _global_Content_Type[counter][0], _global_Content_Type[counter][1]));
            counter++;
        }
        if (strstr(line, CONN_TYPE) != NULL) //#Content-Type
        {
            content_typeflag = 1;
        }
    }
}

/***********************************************************************************
*** main function : start point of the webserver
*** doesnot take any arguments for now, but for furture use the parameters are mentioned
***********************************************************************************/

int main(int argc, char* argv[])
{
    int socket_desc, client_sock, c, *new_sock;
    struct sockaddr_in server, client;
    
    if (!readWebConfig()) // using NOt since it is faster that comparator operation
    {
        printf("Error in Web Config, Please restart your webserver again\n");
        exit(0);
    }

    //Create socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1) {
        printf("Could not create socket");
    }
    puts("Socket created");

    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(_global_PortNumber);

    //Bind
    if (bind(socket_desc, (struct sockaddr*)&server, sizeof(server)) < 0) {
        //print the error message
        perror("bind failed. Error");
        return 1;
    }
    puts("bind done");

    //Listen
    listen(socket_desc, 10);

    //Accept and incoming connection
    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);
    sem_init(&AcceptConnection,0,10);
    while ((client_sock = accept(socket_desc, (struct sockaddr*)&client, (socklen_t*)&c))) {
        puts("Connection accepted");

        pthread_t sniffer_thread;
        new_sock = malloc(1);
        *new_sock = client_sock;

        if (pthread_create(&sniffer_thread, NULL, connection_handler, (void*)new_sock) < 0) {
            perror("could not create thread");
            return 1;
        }

        //Now join the thread , so that we dont terminate before the thread
        //pthread_join( sniffer_thread , NULL); //dont join as pthread_detach is used.
        puts("Handler assigned");
	//sem_wait(&AcceptConnection);
    }

    if (client_sock < 0) {
        perror("accept failed");
        return 1;
    }

    return 0;
}

/***********************************************************************************
*** sendFileToClient function : Reads the file in chunks and sends to the client
*** sock: the socket value from which the tcp connection is extablished - through which the file will be sent 
*** fileSize: the filesize of the file which is being sent 
*** fileName : the name of the file
***********************************************************************************/

void sendFileToClient(int sock, int fileSize, char* fileName)
{
    char buf[MAXLIMIT];
    memset(buf, '\0', MAXLIMIT);
    DEBUG(printf("HERE16\n"));
    int fileSizeCounter = 0, readBytes = 0, sendBytes = 0;
    FILE* fileOpen = NULL;
    fileOpen = fopen(fileName, "r"); //501error
    while (fileSizeCounter < fileSize) {
        readBytes = fread(buf, 1, MAXLIMIT, fileOpen);
        //Send the message back to client
        sendBytes = write(sock, buf, readBytes);
        fileSizeCounter += sendBytes;
        memset(buf, '\0', MAXLIMIT);
    }

    fclose(fileOpen);
}

/***********************************************************************************
*** connection_handler function : handler for every thread created/forked
*** socket_desc : is the socket ID through which the connection was established
***********************************************************************************/

void* connection_handler(void* socket_desc)
{
    //Get the socket descriptor
    int sock = *(int*)socket_desc;
    pthread_detach(pthread_self());
    int read_size;
    char *message, buf[MAXLIMIT];
    memset(buf, '\0', MAXLIMIT);

    tcpReplyPacketInfo* replyPacketInfo = NULL;
    replyPacketInfo = (tcpReplyPacketInfo*)malloc(sizeof(tcpReplyPacketInfo));
    memset(replyPacketInfo, '\0', sizeof(tcpReplyPacketInfo));

    fd_set readfds;
    struct timeval tv;
    int retval;

    int waitTime = 2;
    int keepAliveFlag = 0;
    do {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        /* Wait up to 35ms from the server */

        tv.tv_sec = waitTime;
        tv.tv_usec = 0;
        retval = select(sock + 1, &readfds, NULL, NULL, &tv);
        if (retval == -1)
            perror("select()");
        else if (retval && (FD_ISSET(sock, &readfds))) {

            //Receive a message from client
            if ((read_size = recv(sock, buf, MAXLIMIT, 0)) > 0) {

                //DEBUG(
printf("\n*************************************************\nthis is what i got : \n%s\n\n***************************************************\n", buf);//);
                printf("*************\nSockd value: %d\n******************\n", sock);
				char * tempStr = (char *)malloc(strlen(buf));
				strcpy(tempStr, buf);
				int pipelineFlag =0;
				do
				{	
					pipelineFlag=0;
				        memset(replyPacketInfo, '\0', sizeof(tcpReplyPacketInfo));
					printf("Before ?????????????  tempStr : %s",tempStr);
					parseHttp(tempStr, replyPacketInfo);
					tempStr++;
					printf("After ?????????????  tempStr : %s",tempStr);
					DEBUG(printf("HERE10\n"));
					if (replyPacketInfo->errorFlag != 1) {
						DEBUG(printf("HERE11\n"));
						memset(buf, '\0', MAXLIMIT);
						DEBUG(printf("HERE12\n"));
						char * tempStr = (char*)malloc(strlen(CONNECTION_KEEP_ALIVE));

						if (replyPacketInfo->connectionAliveFlag) {
							DEBUG(printf("COnnection is Alive :%d\n", replyPacketInfo->connectionAliveFlag));
							waitTime = _global_KeepAliveTime;
							keepAliveFlag = 1;
							strcpy(tempStr,CONNECTION_KEEP_ALIVE);
						}
						else {
							DEBUG(printf("COnnection is closed :%d\n", replyPacketInfo->connectionAliveFlag));
							waitTime = 2;
							keepAliveFlag = 0;
							strcpy(tempStr,CONNECTION_CLOSE);
						}
						
						sprintf(buf, "%s %s\nContent-Type: %s\nContent-Length: %d\n%s\n\n", replyPacketInfo->httpVersion, OK_200, replyPacketInfo->fileType, replyPacketInfo->contentLength, tempStr);
						printf("%s\n", buf);
						DEBUG(printf("HERE13\n"));
						write(sock, buf, strlen(buf));
						DEBUG(printf("HERE14\n"));
						if(replyPacketInfo->isPostMethod)
						{
							int postDataLen = strlen(replyPacketInfo->postContent);
							sendFileToClient(sock, replyPacketInfo->contentLength-postDataLen, replyPacketInfo->fileName);
							write(sock, replyPacketInfo->postContent, postDataLen);
							DEBUG(printf("HERE15.0\n"));
						}else
						{
							sendFileToClient(sock, replyPacketInfo->contentLength, replyPacketInfo->fileName);
							DEBUG(printf("HERE15.1\n"));
						}
						DEBUG(printf("HERE15.2\n"));
					}
					else {
						char htmlbody[MINLIMIT];
						memset(htmlbody, '\0', MINLIMIT);
						sprintf(htmlbody, "<html><body>%s</body></html>", replyPacketInfo->errorReason);
						sprintf(buf, "HTTP/1.1 %s\nContent-Type: %s\nContent-Length: %d\n%s\n\n%s", replyPacketInfo->errorMessage, "text/html", (int)strlen(htmlbody), CONNECTION_KEEP_ALIVE, htmlbody);
						printf("%s\n", buf);
						DEBUG(printf("HERE13\n"));
						write(sock, buf, strlen(buf));
						DEBUG(printf("HERE14\n"));
						waitTime = 1;
						keepAliveFlag = 0;
					}
					DEBUG(printf("HERE15.3\n"));
					if(strstr(tempStr,"GET") != NULL){
						tempStr = strstr(tempStr,"GET");
						pipelineFlag=1;
						}
					else if(strstr(tempStr,"POST") != NULL){
						tempStr = strstr(tempStr,"POST");
						pipelineFlag=1;
						}
				}while (pipelineFlag);
            }
				
            if (read_size == 0) {
                fflush(stdout);
            }
            else if (read_size == -1) {
                perror("recv failed");
            }
        }
        else 
        {
          waitTime = 2;
          keepAliveFlag = 0;
        }
    } while (keepAliveFlag);
    //sem_post(&AcceptConnection);
    printf("HERE16.4\n");
    fflush(stdout);
    //Free the socket pointer
    free(socket_desc);
    close(sock);
    printf("HERE17\n");
    fflush(stdout);
    pthread_exit(0);
    return NULL;
}

