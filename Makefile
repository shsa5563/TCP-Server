#Makefile
CC = gcc
INCLUDE = /usr/lib
LIBS = -lpthread -lrt 
OBJS = 
HEADERS= .
all:  webserver

webserver:
	$(CC)  -o webserver webserver.c -I$(HEADERS) $(CFLAGS) $(LIBS) 

clean:
	rm -f  webserver 

