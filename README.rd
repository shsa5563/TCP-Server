
Author: Shekhar 
C socket server example, handles multiple clients using threads
reference:http:www.binarytides.comserver-client-example-c-sockets-linux

***************** What the code will do *************************

1. Accept Multiple Clients
2. Server COnnection-KeepAlive with appropriate time in seconds defined in ws.conf 
3. Pipelining of the GET and POST methods only 
4. Serves both GET and POST methods. For POST method the posted data is added to the start of the body of the index.html page using jQuery.
5. All the error codes (400, 404, 500, 501)


**************** Functions in the Code **************************

->getFileNameFromDirectoryString function gets the fileName from the lengthy DIrectory names 
 ex : imagesfileName.txt this function gets filename fileName.txt

->show_dir_content : A recursive function to search a file of interest recursively in a given directory
 reference : https:stackoverflow.comquestions4204666how-to-list-files-in-a-directory-in-a-c-program
 path : the the path in which the search  has to be done
 fileName: the file Name to search
 fileExixts: is a global flag which will be updated respective to the existence of the file

->parseHttp function : is used to parse the HTTP request and check if method,filename, file extensions are all appropriate
 httpRequest1 : is the httprequest 
 replyPacketInfo: is pointer type of tcpReplyPacketInfo, to which the respective parsed data will be updated

->compare_filename_ext function : compares if the file extension exists in the ws.conf; replies -1 if not found
 filename : the filename with extension  
 replyfileType:  will be updated with the appropirate ws.conf filetype for browsers if the file exists

->readWebConfig function : parses the ws.conf file and put the extracted values to global variables
 the global variables are used to configure the webserver; thi fuction is called once after the main starts

->sendFileToClient function : Reads the file in chunks and sends to the client
 sock: the socket value from which the tcp connection is extablished - through which the file will be sent 
 fileSize: the filesize of the file which is being sent 
 fileName : the name of the file

->connection_handler function : handler for every thread createdforked
 socket_desc : is the socket ID through which the connection was established

********************* How TO use the code ***************************

To use the Webserver Example: 
1. Run the ./webserver 
2. For the Client either use telnet or the Browser
   1. Post Method: (echo -en "POST /index.html HTTP/1.1\r\nHost: localhost\r\nConnection: Keepalive\r\n\r\nPOSTDATA”; sleep 10) | telnet 127.0.0.1 8888
   
   2. Get Method PipeLine: (echo -en "GET /index.html HTTP/1.1\r\nHost: localhost\r\nConnection: Keepalive\
r\n\r\nGET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n"; sleep 10) | telnet
127.0.0.1 8888

   3. Get Method Browser: http://localhost:8888/index.html or http://localhost:8888


